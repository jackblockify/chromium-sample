// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/store_file_task.h"

#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/threading/scoped_blocking_call.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"

namespace {
bool g_skip_copying_for_testing = false;
}  // namespace

namespace webshare {

StoreFileTask::StoreFileTask(base::FilePath filename,
                             blink::mojom::SharedFilePtr file,
                             uint64_t& available_space,
                             blink::mojom::ShareService::ShareCallback callback)
    : filename_(std::move(filename)),
      file_(std::move(file)),
      available_space_(available_space),
      callback_(std::move(callback)),
      read_pipe_watcher_(FROM_HERE,
                         mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC) {
  // |filename| is generated by GenerateFileName().
  DCHECK(!filename_.ReferencesParent());
  DCHECK(filename_.BaseName().MaybeAsASCII().find("share") >= 0);
}

StoreFileTask::~StoreFileTask() = default;

void StoreFileTask::Start() {
  {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::WILL_BLOCK);
    output_file_.Initialize(
        filename_, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  }

  if (g_skip_copying_for_testing) {
    std::move(callback_).Run(blink::mojom::ShareError::OK);
    return;
  }

  StartRead();
}

void StoreFileTask::StartRead() {
  constexpr size_t kDataPipeCapacity = 65536;

  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = kDataPipeCapacity;

  mojo::ScopedDataPipeProducerHandle producer_handle;
  MojoResult rv = CreateDataPipe(&options, producer_handle, consumer_handle_);
  if (rv != MOJO_RESULT_OK) {
    std::move(callback_).Run(blink::mojom::ShareError::INTERNAL_ERROR);
    return;
  }

  mojo::Remote<blink::mojom::Blob> blob(std::move(file_->blob->blob));
  blob->ReadAll(std::move(producer_handle),
                receiver_.BindNewPipeAndPassRemote());
}

// static
void StoreFileTask::SkipCopyingForTesting() {
  g_skip_copying_for_testing = true;
}

void StoreFileTask::OnDataPipeReadable(MojoResult result) {
  if (result != MOJO_RESULT_OK) {
    if (!received_all_data_) {
      std::move(callback_).Run(blink::mojom::ShareError::INTERNAL_ERROR);
    }
    return;
  }

  while (true) {
    size_t buffer_num_bytes;
    const void* buffer;
    MojoResult pipe_result = consumer_handle_->BeginReadData(
        &buffer, &buffer_num_bytes, MOJO_READ_DATA_FLAG_NONE);
    if (pipe_result == MOJO_RESULT_SHOULD_WAIT)
      return;

    if (pipe_result == MOJO_RESULT_FAILED_PRECONDITION) {
      // Pipe closed.
      if (!received_all_data_) {
        std::move(callback_).Run(blink::mojom::ShareError::INTERNAL_ERROR);
      }
      return;
    }
    if (pipe_result != MOJO_RESULT_OK) {
      std::move(callback_).Run(blink::mojom::ShareError::INTERNAL_ERROR);
      return;
    }

    // Defend against compromised renderer process sending too much data.
    int buffer_num_bytes_int = base::saturated_cast<int>(buffer_num_bytes);
    if (buffer_num_bytes > total_bytes_ - bytes_received_ ||
        output_file_.WriteAtCurrentPos(static_cast<const char*>(buffer),
                                       buffer_num_bytes_int) !=
            buffer_num_bytes_int) {
      std::move(callback_).Run(blink::mojom::ShareError::INTERNAL_ERROR);
      return;
    }
    bytes_received_ += buffer_num_bytes;
    DCHECK_LE(bytes_received_, total_bytes_);

    consumer_handle_->EndReadData(buffer_num_bytes);
    if (bytes_received_ == total_bytes_) {
      received_all_data_ = true;
      if (received_on_complete_)
        OnSuccess();
      return;
    }
  }
}

void StoreFileTask::OnSuccess() {
  DCHECK_EQ(bytes_received_, total_bytes_);

  std::move(callback_).Run(blink::mojom::ShareError::OK);
}

void StoreFileTask::OnCalculatedSize(uint64_t total_size,
                                     uint64_t expected_content_size) {
  DCHECK_EQ(total_size, expected_content_size);

  if (expected_content_size > *available_space_ ||
      expected_content_size != file_->blob->size) {
    VLOG(1) << "Share too large: " << expected_content_size << " bytes";
    std::move(callback_).Run(blink::mojom::ShareError::PERMISSION_DENIED);
    return;
  }

  *available_space_ -= expected_content_size;
  total_bytes_ = expected_content_size;

  if (expected_content_size == 0) {
    received_all_data_ = true;
    return;
  }

  read_pipe_watcher_.Watch(
      consumer_handle_.get(), MOJO_HANDLE_SIGNAL_READABLE,
      base::BindRepeating(&StoreFileTask::OnDataPipeReadable,
                          weak_ptr_factory_.GetWeakPtr()));
}

void StoreFileTask::OnComplete(int32_t status, uint64_t data_length) {
  if (status != net::OK || data_length != total_bytes_) {
    std::move(callback_).Run(blink::mojom::ShareError::INTERNAL_ERROR);
    return;
  }

  received_on_complete_ = true;
  if (received_all_data_)
    OnSuccess();
}

}  // namespace webshare
