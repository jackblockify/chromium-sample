// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/camera_preview/video_stream_coordinator.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/media_preview/camera_preview/video_stream_view.h"
#include "components/media_effects/test/fake_video_source.h"
#include "media/capture/video_capture_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/canvas.h"

using testing::_;
using testing::Mock;
using testing::Sequence;

class VideoStreamCoordinatorTest : public TestWithBrowserView {
 protected:
  VideoStreamCoordinatorTest()
      : TestWithBrowserView(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        video_source_receiver_(&fake_video_source_) {}

  void SetUp() override {
    TestWithBrowserView::SetUp();
    parent_view_ = std::make_unique<views::View>();
    coordinator_ = std::make_unique<VideoStreamCoordinator>(
        *parent_view_, media_preview_metrics::Context(
                           media_preview_metrics::UiLocation::kPermissionPrompt,
                           media_preview_metrics::PreviewType::kCamera));
  }

  void TearDown() override {
    coordinator_.reset();
    parent_view_.reset();
    TestWithBrowserView::TearDown();
  }

  static media::VideoCaptureDeviceInfo GetVideoCaptureDeviceInfo() {
    media::VideoCaptureDeviceDescriptor descriptor;
    descriptor.device_id = "device_id";

    media::VideoCaptureDeviceInfo device_info(descriptor);
    device_info.supported_formats = {
        {{160, 120}, 15.0, media::PIXEL_FORMAT_I420},
        {{160, 120}, 30.0, media::PIXEL_FORMAT_NV12},
        {{640, 480}, 30.0, media::PIXEL_FORMAT_NV12},
        {{640, 480}, 30.0, media::PIXEL_FORMAT_I420},
        {{3840, 2160}, 30.0, media::PIXEL_FORMAT_Y16},
        {{844, 400}, 30.0, media::PIXEL_FORMAT_NV12},
        {{1280, 720}, 30.0, media::PIXEL_FORMAT_I420}};
    return device_info;
  }

  void TriggerPaint() {
    CHECK(parent_view_->children().size() == 1);
    auto* video_stream_view = coordinator_->GetVideoStreamView();
    gfx::Canvas canvas;
    video_stream_view->OnPaint(&canvas);
  }

  std::unique_ptr<views::View> parent_view_;
  std::unique_ptr<VideoStreamCoordinator> coordinator_;

  FakeVideoSource fake_video_source_;
  mojo::Receiver<video_capture::mojom::VideoSource> video_source_receiver_;

  base::HistogramTester histogram_tester_;
};

TEST_F(VideoStreamCoordinatorTest, ConnectToFrameHandlerAndReceiveFrames) {
  mojo::Remote<video_capture::mojom::VideoSource> video_source;
  video_source_receiver_.Bind(video_source.BindNewPipeAndPassReceiver());
  coordinator_->ConnectToDevice(GetVideoCaptureDeviceInfo(),
                                std::move(video_source));

  coordinator_->GetVideoStreamView()->SetPreferredSize(gfx::Size{250, 180});
  coordinator_->GetVideoStreamView()->SizeToPreferredSize();

  EXPECT_TRUE(fake_video_source_.WaitForCreatePushSubscription());
  EXPECT_TRUE(fake_video_source_.WaitForPushSubscriptionActivated());

  base::test::TestFuture<void> got_frame;
  coordinator_->SetFrameReceivedCallbackForTest(
      got_frame.GetRepeatingCallback());
  // Send 18 frames over a simulated second
  for (size_t i = 0; i < 18; ++i) {
    fake_video_source_.SendFrame();
    task_environment()->AdvanceClock(base::Milliseconds(55.8));
    EXPECT_TRUE(got_frame.WaitAndClear());
    // Paint every other frame.
    if (i % 2) {
      TriggerPaint();
    }
  }

  coordinator_->Stop();
  EXPECT_TRUE(fake_video_source_.WaitForPushSubscriptionClosed());

  // The selected pixel height is 500, so it will be logged in the 406 bucket.
  histogram_tester_.ExpectUniqueSample(
      "MediaPreviews.UI.Permissions.Camera.PixelHeight",
      /*bucket_min_value=*/406, 1);
  histogram_tester_.ExpectUniqueSample(
      "MediaPreviews.UI.Preview.Permissions.Video.ExpectedFPS",
      /*bucket_min_value=*/30, 1);
  histogram_tester_.ExpectUniqueSample(
      "MediaPreviews.UI.Preview.Permissions.Video.ActualFPS",
      /*bucket_min_value=*/18, 1);
  histogram_tester_.ExpectUniqueSample(
      "MediaPreviews.UI.Preview.Permissions.Video.RenderedPercent",
      /*bucket_min_value=*/50, 1);
}
