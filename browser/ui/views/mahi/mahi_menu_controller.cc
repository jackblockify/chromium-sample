// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/mahi/mahi_menu_controller.h"

#include <memory>

#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/mahi/mahi_web_contents_manager.h"
#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_card_controller.h"
#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_ui_controller.h"
#include "chrome/browser/ui/views/mahi/mahi_condensed_menu_view.h"
#include "chrome/browser/ui/views/mahi/mahi_menu_constants.h"
#include "chrome/browser/ui/views/mahi/mahi_menu_view.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "chromeos/components/mahi/public/cpp/mahi_switches.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/views/view_utils.h"

namespace chromeos::mahi {

MahiMenuController::MahiMenuController(
    ReadWriteCardsUiController& read_write_cards_ui_controller)
    : read_write_cards_ui_controller_(read_write_cards_ui_controller) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // MahiMediaAppEventsProxy is initialized only in ash chrome.
  CHECK(chromeos::MahiMediaAppEventsProxy::Get());
  chromeos::MahiMediaAppEventsProxy::Get()->AddObserver(this);
#endif
}

MahiMenuController::~MahiMenuController() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  CHECK(chromeos::MahiMediaAppEventsProxy::Get());
  chromeos::MahiMediaAppEventsProxy::Get()->RemoveObserver(this);
#endif
}

void MahiMenuController::OnContextMenuShown(Profile* profile) {}

void MahiMenuController::OnTextAvailable(const gfx::Rect& anchor_bounds,
                                         const std::string& selected_text,
                                         const std::string& surrounding_text) {
  if (!chromeos::MahiManager::IsSupportedWithCorrectFeatureKey() ||
      !::mahi::MahiWebContentsManager::Get()->GetPrefValue()) {
    return;
  }

  bool page_distillable =
      ::mahi::MahiWebContentsManager::Get()->IsFocusedPageDistillable();

  // Records metric of whether the page is distillable when Mahi menu is
  // requested to show.
  base::UmaHistogramBoolean(kMahiContextMenuDistillableHistogram,
                            page_distillable);

  // Only shows mahi menu for distillable pages or when the switch
  // `kUseFakeMahiManager` is enabled.
  if (!page_distillable && !base::CommandLine::ForCurrentProcess()->HasSwitch(
                               chromeos::switches::kUseFakeMahiManager)) {
    return;
  }

  if (features::IsMagicBoostEnabled() &&
      MagicBoostCardController::Get()->ShouldQuickAnswersAndMahiShowOptIn()) {
    // TODO(b/344037679): Remove this logic when we use
    // `ReadWriteCardsManagerImpl` to fetch the controller.
    MagicBoostCardController::Get()->ShowOptInUi(anchor_bounds);
    return;
  }

  if (selected_text.empty()) {
    menu_widget_ = MahiMenuView::CreateWidget(anchor_bounds);
    menu_widget_->ShowInactive();
    return;
  }

  // If there is selected text, we will show the condensed Mahi view alongside
  // quick answers.
  read_write_cards_ui_controller_->SetMahiUi(
      std::make_unique<MahiCondensedMenuView>());
}

void MahiMenuController::OnAnchorBoundsChanged(const gfx::Rect& anchor_bounds) {
  if (!menu_widget_ || !menu_widget_->GetContentsView()) {
    return;
  }

  views::AsViewClass<MahiMenuView>(menu_widget_->GetContentsView())
      ->UpdateBounds(anchor_bounds);
}

void MahiMenuController::OnDismiss(bool is_other_command_executed) {
  if (menu_widget_ && !menu_widget_->IsActive()) {
    menu_widget_.reset();
  }

  if (features::IsMagicBoostEnabled()) {
    // TODO(b/344037679): Remove this logic when we use
    // `ReadWriteCardsManagerImpl` to fetch the controller.
    MagicBoostCardController::Get()->CloseOptInUi();
  }

  read_write_cards_ui_controller_->RemoveMahiUi();
}

void MahiMenuController::OnPdfContextMenuShown(const gfx::Rect& anchor) {
  if (!chromeos::MahiManager::IsSupportedWithCorrectFeatureKey() ||
      !::mahi::MahiWebContentsManager::Get()->GetPrefValue()) {
    return;
  }

  menu_widget_ =
      MahiMenuView::CreateWidget(anchor, MahiMenuView::Surface::kMediaApp);
  menu_widget_->ShowInactive();
}

void MahiMenuController::OnPdfContextMenuHide() {
  OnDismiss(false);
}

base::WeakPtr<MahiMenuController> MahiMenuController::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace chromeos::mahi
