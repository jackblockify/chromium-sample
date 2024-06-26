// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_save_card_delegate_android.h"

#include "components/browser_ui/device_lock/android/device_lock_bridge.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

namespace autofill {

AutofillSaveCardDelegateAndroid::AutofillSaveCardDelegateAndroid(
    absl::variant<AutofillClient::LocalSaveCardPromptCallback,
                  AutofillClient::UploadSaveCardPromptCallback> callback,
    AutofillClient::SaveCreditCardOptions options,
    content::WebContents* web_contents)
    : AutofillSaveCardDelegate(std::move(callback), options) {
  device_lock_bridge_ = std::make_unique<DeviceLockBridge>();
  web_contents_ = web_contents;
}

void AutofillSaveCardDelegateAndroid::SetDeviceLockBridgeForTesting(
    std::unique_ptr<DeviceLockBridge> device_lock_bridge) {
  device_lock_bridge_ = std::move(device_lock_bridge);
}

AutofillSaveCardDelegateAndroid::~AutofillSaveCardDelegateAndroid() = default;

void AutofillSaveCardDelegateAndroid::GatherAdditionalConsentIfApplicable(
    AutofillClient::UserProvidedCardDetails user_provided_details) {
  device_lock_bridge_->LaunchDeviceLockUiIfNeededBeforeRunningCallback(
      web_contents_->GetNativeView()->GetWindowAndroid(),
      base::BindOnce(&AutofillSaveCardDelegateAndroid::OnAfterDeviceLockUi,
                     weak_ptr_factory_.GetWeakPtr(), user_provided_details));
}

void AutofillSaveCardDelegateAndroid::OnAfterDeviceLockUi(
    AutofillClient::UserProvidedCardDetails user_provided_details,
    bool is_device_lock_requirement_met) {
  OnFinishedGatheringConsent(
      /*user_decision=*/is_device_lock_requirement_met
          ? AutofillClient::SaveCardOfferUserDecision::kAccepted
          : AutofillClient::SaveCardOfferUserDecision::kIgnored,
      is_device_lock_requirement_met
          ? user_provided_details
          : AutofillClient::UserProvidedCardDetails());
}

}  // namespace autofill
