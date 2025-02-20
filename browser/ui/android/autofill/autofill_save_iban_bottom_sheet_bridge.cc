// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_save_iban_bottom_sheet_bridge.h"

#include <memory>

#include "base/android/jni_string.h"
#include "chrome/browser/ui/android/autofill/autofill_save_iban_delegate.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/AutofillSaveIbanBottomSheetBridge_jni.h"

namespace autofill {

AutofillSaveIbanBottomSheetBridge::AutofillSaveIbanBottomSheetBridge(
    ui::WindowAndroid* window_android,
    TabModel* tab_model) {
  CHECK(window_android);
  CHECK(tab_model);
  java_autofill_save_iban_bottom_sheet_bridge_ =
      Java_AutofillSaveIbanBottomSheetBridge_Constructor(
          base::android::AttachCurrentThread(), reinterpret_cast<jlong>(this),
          window_android->GetJavaObject(), tab_model->GetJavaObject());
}

AutofillSaveIbanBottomSheetBridge::~AutofillSaveIbanBottomSheetBridge() {
  if (java_autofill_save_iban_bottom_sheet_bridge_) {
    Java_AutofillSaveIbanBottomSheetBridge_destroy(
        base::android::AttachCurrentThread(),
        java_autofill_save_iban_bottom_sheet_bridge_);
  }
}

void AutofillSaveIbanBottomSheetBridge::RequestShowContent(
    std::u16string_view iban_label,
    std::unique_ptr<AutofillSaveIbanDelegate> delegate) {
  JNIEnv* env = base::android::AttachCurrentThread();
  save_iban_delegate_ = std::move(delegate);
  Java_AutofillSaveIbanBottomSheetBridge_requestShowContent(
      env, java_autofill_save_iban_bottom_sheet_bridge_, iban_label);
}

void AutofillSaveIbanBottomSheetBridge::OnUiAccepted(
    JNIEnv* env,
    const std::u16string& user_provided_nickname) {
  if (save_iban_delegate_) {
    save_iban_delegate_->OnUiAccepted(
        base::BindOnce(
            &AutofillSaveIbanBottomSheetBridge::ResetSaveIbanDelegate,
            base::Unretained(this)),
        user_provided_nickname);
  }
}

void AutofillSaveIbanBottomSheetBridge::OnUiCanceled(JNIEnv* env) {
  if (save_iban_delegate_) {
    save_iban_delegate_->OnUiCanceled();
  }
  ResetSaveIbanDelegate();
}

void AutofillSaveIbanBottomSheetBridge::OnUiIgnored(JNIEnv* env) {
  if (save_iban_delegate_) {
    save_iban_delegate_->OnUiIgnored();
  }
  ResetSaveIbanDelegate();
}

void AutofillSaveIbanBottomSheetBridge::ResetSaveIbanDelegate() {
  save_iban_delegate_.reset(nullptr);
}

}  // namespace autofill
