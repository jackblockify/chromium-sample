// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/edu_coexistence_login_screen.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screen_manager.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/ui/login_display_host_mojo.h"
#include "chrome/browser/ash/login/ui/oobe_ui_dialog_delegate.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/browser/ui/webui/signin/ash/inline_login_dialog_onboarding.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

// static
constexpr StaticOobeScreenId EduCoexistenceLoginScreen::kScreenId;

// static
EduCoexistenceLoginScreen* EduCoexistenceLoginScreen::Get(
    ScreenManager* screen_manager) {
  return static_cast<EduCoexistenceLoginScreen*>(
      screen_manager->GetScreen(EduCoexistenceLoginScreen::kScreenId));
}

// static
std::string EduCoexistenceLoginScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::DONE:
      return "Done";
    case Result::SKIPPED:
      return BaseScreen::kNotApplicable;
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
}

EduCoexistenceLoginScreen::EduCoexistenceLoginScreen(
    const ScreenExitCallback& exit_callback)
    : BaseScreen(EduCoexistenceLoginScreen::kScreenId,
                 OobeScreenPriority::DEFAULT),
      exit_callback_(exit_callback) {
  observed_login_display_host_.Observe(LoginDisplayHost::default_host());
}

EduCoexistenceLoginScreen::~EduCoexistenceLoginScreen() {}

bool EduCoexistenceLoginScreen::MaybeSkip(WizardContext& context) {
  if (context.skip_post_login_screens_for_tests ||
      !ProfileManager::GetActiveUserProfile()->IsChild()) {
    exit_callback_.Run(Result::SKIPPED);
    return true;
  }

  return false;
}

void EduCoexistenceLoginScreen::ShowImpl() {
  LoginDisplayHost* host = LoginDisplayHost::default_host();
  DCHECK(host);
  OobeUI* oobe_ui = host->GetOobeUI();
  DCHECK(oobe_ui);
  DCHECK(!dialog_delegate_);

  InlineLoginDialogOnboarding* dialog = InlineLoginDialogOnboarding::Show(
      oobe_ui->GetViewSize(),
      /* parent window */ oobe_ui->GetTopLevelNativeWindow(),
      base::BindOnce(exit_callback_, Result::DONE));

  dialog_delegate_ =
      std::make_unique<InlineLoginDialogOnboarding::Delegate>(dialog);
  dialog_delegate_->UpdateDialogBounds(
      oobe_ui->GetNativeView()->GetBoundsInScreen());
  // RestartOnboarding() depends on this signal.
  // In normal OOBE UI it is generated by display_manager.js. But
  // this screen does not use display_manager.js, so we just send it here.
  oobe_ui->CurrentScreenChanged(kScreenId);

  if (ash::features::AreLocalPasswordsEnabledForConsumers()) {
    if (context()->extra_factors_token) {
      session_refresher_ = AuthSessionStorage::Get()->KeepAlive(
          context()->extra_factors_token.value());
    }
  }
}

void EduCoexistenceLoginScreen::HideImpl() {
  if (dialog_delegate_) {
    dialog_delegate_->CloseWithoutCallback();
    dialog_delegate_.reset();
  }
  session_refresher_.reset();
}

void EduCoexistenceLoginScreen::WebDialogViewBoundsChanged(
    const gfx::Rect& bounds) {
  if (!dialog_delegate_)
    return;

  dialog_delegate_->UpdateDialogBounds(bounds);
}

}  // namespace ash
