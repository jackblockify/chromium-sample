// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scanning/lorgnette_notification_controller.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/device_event_log/device_event_log.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "url/gurl.h"

namespace ash {

namespace {
const char kNotifierId[] = "scanning.dlc";
const char kNotificationId[] = "scanning_dlc_notification";
const char kDlcId[] = "sane-backends-pfu";

std::unique_ptr<message_center::Notification> NewNotification(
    int title,
    int message,
    ui::ColorId color_id,
    const gfx::VectorIcon& image) {
  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.accent_color_id = color_id;
  rich_notification_data.vector_small_image = &image;
  return std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId,
      l10n_util::GetStringUTF16(title), l10n_util::GetStringUTF16(message),
      ui::ImageModel(),  // icon
      l10n_util::GetStringUTF16(IDS_SCANNING_DLC_NOTIFICATION_DISPLAY_SOURCE),
      GURL(),  // origin_url
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierId,
                                 NotificationCatalogName::kDocumentScanning),
      rich_notification_data, new message_center::NotificationDelegate());
}
}  // namespace

LorgnetteNotificationController::LorgnetteNotificationController(
    Profile* profile)
    : dlc_observer_(this), current_state_(DlcState::kIdle), profile_(profile) {
  DCHECK(profile);
  dlc_observer_.Observe(DlcserviceClient::Get());
}

LorgnetteNotificationController::~LorgnetteNotificationController() = default;

void LorgnetteNotificationController::OnDlcStateChanged(
    const dlcservice::DlcState& dlc_state) {
  if (dlc_state.id() != kDlcId) {
    return;
  }

  switch (dlc_state.state()) {
    case dlcservice::DlcState::INSTALLED:
      // Only set state to kInstalledSuccessfully if previous start was
      // kInstalling to send a notification only if the DLC is downloading and
      // not just mounting.
      if (current_state_ == DlcState::kInstalling) {
        current_state_ = DlcState::kInstalledSuccessfully;
      } else {
        current_state_ = DlcState::kIdle;
      }
      PRINTER_LOG(EVENT) << "Scanning DLC ID: " << kDlcId
                         << " installed successfully";
      break;
    case dlcservice::DlcState::INSTALLING:
      current_state_ = DlcState::kInstalling;
      break;
    case dlcservice::DlcState::NOT_INSTALLED:
      current_state_ = DlcState::kInstallError;
      PRINTER_LOG(ERROR) << "Scanning DLC ID: " << kDlcId
                         << " exited with error: "
                         << dlc_state.last_error_code();
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  std::unique_ptr<message_center::Notification> notification =
      CreateNotification();
  DisplayNotification(std::move(notification));
}

std::unique_ptr<message_center::Notification>
LorgnetteNotificationController::CreateNotification() {
  switch (current_state_) {
    case DlcState::kInstalledSuccessfully:
      return NewNotification(IDS_SCANNING_DLC_NOTIFICATION_INSTALLED_TITLE,
                             IDS_EMPTY_STRING, cros_tokens::kCrosSysPrimary,
                             kNotificationPrintingIcon);
    case DlcState::kInstalling:
      return NewNotification(IDS_SCANNING_DLC_NOTIFICATION_INSTALLING_TITLE,
                             IDS_EMPTY_STRING, cros_tokens::kCrosSysPrimary,
                             kNotificationPrintingIcon);
    case DlcState::kInstallError:
      return NewNotification(
          IDS_SCANNING_DLC_NOTIFICATION_INSTALL_FAILED_TITLE,
          IDS_SCANNING_DLC_NOTIFICATION_INSTALL_FAILED_MESSAGE,
          cros_tokens::kCrosSysError, kNotificationPrintingWarningIcon);
    case DlcState::kIdle:
      return nullptr;
  }
  NOTREACHED_IN_MIGRATION();
}

void LorgnetteNotificationController::DisplayNotification(
    std::unique_ptr<message_center::Notification> notification) {
  NotificationDisplayService* display_service =
      NotificationDisplayServiceFactory::GetForProfile(profile_);
  if (current_state_ == DlcState::kIdle) {
    display_service->Close(NotificationHandler::Type::TRANSIENT,
                           kNotificationId);
  } else {
    display_service->Display(NotificationHandler::Type::TRANSIENT,
                             *notification,
                             /*metadata=*/nullptr);
  }
}

LorgnetteNotificationController::DlcState
LorgnetteNotificationController::current_state_for_testing() {
  return current_state_;
}

}  // namespace ash
