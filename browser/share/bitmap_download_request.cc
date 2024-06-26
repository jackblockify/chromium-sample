// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "base/android/jni_string.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_request_utils.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/android/java_bitmap.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/BitmapDownloadRequest_jni.h"

using base::android::ConvertJavaStringToUTF16;
using base::android::JavaParamRef;

static void JNI_BitmapDownloadRequest_DownloadBitmap(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_filename,
    const JavaParamRef<jobject>& j_bitmap) {
  std::u16string filename(ConvertJavaStringToUTF16(env, j_filename));
  SkBitmap bitmap =
      gfx::CreateSkBitmapFromJavaBitmap(gfx::JavaBitmap(j_bitmap));

  const GURL data_url = GURL(webui::GetBitmapDataUrl(bitmap));

  content::DownloadManager* download_manager =
      ProfileManager::GetLastUsedProfile()->GetDownloadManager();
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("download_bitmap", R"(
        semantics {
          sender: "Sharing Hub Android"
          description:
            "Download bitmap request sent by chrome browser share component on Android."
          trigger: "User clicks 'download' from Sharing Hub on Android."
          data: "Bitmap generated by Sharing Hub feature."
          destination: LOCAL
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users cannot disable this feature."
          policy_exception_justification:
            "N/A"
        })");
  auto params = std::make_unique<download::DownloadUrlParameters>(
      data_url, traffic_annotation);
  params->set_suggested_name(filename);
  download_manager->DownloadUrl(std::move(params));
}
