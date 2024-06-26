// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/quick_answers_ui_controller.h"

#include <optional>

#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_ui_controller.h"
#include "chrome/browser/ui/quick_answers/quick_answers_controller_impl.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_util.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_view.h"
#include "chrome/browser/ui/quick_answers/ui/rich_answers_definition_view.h"
#include "chrome/browser/ui/quick_answers/ui/rich_answers_translation_view.h"
#include "chrome/browser/ui/quick_answers/ui/rich_answers_unit_conversion_view.h"
#include "chrome/browser/ui/quick_answers/ui/rich_answers_view.h"
#include "chromeos/components/quick_answers/public/cpp/controller/quick_answers_controller.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/new_window_delegate.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_commands.h"          // nogncheck
#include "chrome/browser/ui/browser_finder.h"            // nogncheck
#include "chrome/browser/ui/browser_navigator.h"         // nogncheck
#include "chrome/browser/ui/browser_navigator_params.h"  // nogncheck
#include "chromeos/crosapi/mojom/url_handler.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace {

using quick_answers::QuickAnswer;
using quick_answers::QuickAnswersExitPoint;

constexpr char kFeedbackDescriptionTemplate[] = "#QuickAnswers\nQuery:%s\n";

constexpr char kQuickAnswersSettingsUrl[] =
    "chrome://os-settings/osSearch/search";

// Open the specified URL in a new tab with the specified profile
void OpenUrl(Profile* profile, const GURL& url) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // We always want to open a link in Lacros browser if LacrosOnly is true.
  // `GetPrimary` returns a proper delegate depending on the flag.
  ash::NewWindowDelegate::GetPrimary()->OpenUrl(
      url, ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      ash::NewWindowDelegate::Disposition::kNewForegroundTab);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  NavigateParams navigate_params(
      profile, url,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_FROM_API));
  navigate_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  navigate_params.window_action = NavigateParams::SHOW_WINDOW;
  Navigate(&navigate_params);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

}  // namespace

using chromeos::ReadWriteCardsUiController;

QuickAnswersUiController::QuickAnswersUiController(
    QuickAnswersControllerImpl* controller)
    : controller_(controller) {}

QuickAnswersUiController::~QuickAnswersUiController() = default;

void QuickAnswersUiController::CreateQuickAnswersView(Profile* profile,
                                                      const std::string& title,
                                                      const std::string& query,
                                                      bool is_internal) {
  CreateQuickAnswersViewInternal(
      profile, query,
      {
          .title = title,
          .design = quick_answers::QuickAnswersView::Design::kCurrent,
          // Use `kDefinition` as a placeholder for now. `Design::kCurrent`
          // doesn't care intent.
          // TODO(b/340628664): wire the correct intent.
          .intent = quick_answers::QuickAnswersView::Intent::kDefinition,
          .is_internal = is_internal,
      });
}

void QuickAnswersUiController::CreateQuickAnswersViewForPixelTest(
    Profile* profile,
    const std::string& query,
    quick_answers::QuickAnswersView::Params params) {
  CHECK_IS_TEST();
  CreateQuickAnswersViewInternal(profile, query, params);
}

void QuickAnswersUiController::CreateQuickAnswersViewInternal(
    Profile* profile,
    const std::string& query,
    quick_answers::QuickAnswersView::Params params) {
  // Currently there are timing issues that causes the quick answers view is not
  // dismissed. TODO(updowndota): Remove the special handling after the root
  // cause is found.
  if (IsShowingQuickAnswersView()) {
    LOG(ERROR) << "Quick answers view not dismissed.";
    CloseQuickAnswersView();
  }

  DCHECK(!IsShowingUserConsentView());
  SetActiveQuery(profile, query);

  auto* view = GetReadWriteCardsUiController().SetQuickAnswersUi(
      std::make_unique<quick_answers::QuickAnswersView>(
          params,
          /*controller=*/weak_factory_.GetWeakPtr()));
  quick_answers_view_.SetView(view);
}

void QuickAnswersUiController::CreateRichAnswersView() {
  CHECK(controller_->quick_answer());

  views::UniqueWidgetPtr widget = quick_answers::RichAnswersView::CreateWidget(
      controller_->anchor_bounds(), weak_factory_.GetWeakPtr(),
      *controller_->quick_answer(), *controller_->structured_result());

  if (!widget) {
    // If the rich card widget cannot be created, fall-back to open the query
    // in Google Search.
    OpenUrl(profile_, quick_answers::GetDetailsUrlForQuery(query_));
    controller_->OnQuickAnswerClick();
  }

  rich_answers_widget_ = std::move(widget);
  rich_answers_widget_->Show();
  controller_->SetVisibility(QuickAnswersVisibility::kRichAnswersVisible);
  return;
}

void QuickAnswersUiController::OnQuickAnswersViewPressed() {
  // Route dismissal through |controller_| for logging impressions.
  controller_->DismissQuickAnswers(QuickAnswersExitPoint::kQuickAnswersClick);

  // Trigger the corresponding rich card view if the feature is enabled.
  if (chromeos::features::IsQuickAnswersRichCardEnabled() &&
      controller_->quick_answer() != nullptr) {
    CreateRichAnswersView();
    return;
  }

  OpenUrl(profile_, quick_answers::GetDetailsUrlForQuery(query_));
  controller_->OnQuickAnswerClick();
}

void QuickAnswersUiController::OnGoogleSearchLabelPressed() {
  OpenUrl(profile_, quick_answers::GetDetailsUrlForQuery(query_));

  // Route dismissal through |controller_| for logging impressions.
  controller_->DismissQuickAnswers(QuickAnswersExitPoint::kUnspecified);
}

bool QuickAnswersUiController::CloseQuickAnswersView() {
  if (controller_->GetQuickAnswersVisibility() ==
      QuickAnswersVisibility::kQuickAnswersVisible) {
    GetReadWriteCardsUiController().RemoveQuickAnswersUi();
    return true;
  }
  return false;
}

bool QuickAnswersUiController::CloseRichAnswersView() {
  if (IsShowingRichAnswersView()) {
    rich_answers_widget_->Close();
    return true;
  }
  return false;
}

void QuickAnswersUiController::OnRetryLabelPressed() {
  if (!fake_on_retry_label_pressed_callback_.is_null()) {
    CHECK_IS_TEST();
    fake_on_retry_label_pressed_callback_.Run();
    return;
  }

  controller_->OnRetryQuickAnswersRequest();
}

void QuickAnswersUiController::SetFakeOnRetryLabelPressedCallbackForTesting(
    QuickAnswersUiController::FakeOnRetryLabelPressedCallback
        fake_on_retry_label_pressed_callback) {
  CHECK_IS_TEST();
  CHECK(!fake_on_retry_label_pressed_callback.is_null());
  CHECK(fake_on_retry_label_pressed_callback_.is_null());
  fake_on_retry_label_pressed_callback_ = fake_on_retry_label_pressed_callback;
}

void QuickAnswersUiController::RenderQuickAnswersViewWithResult(
    const QuickAnswer& quick_answer) {
  if (!IsShowingQuickAnswersView())
    return;

  // QuickAnswersView was initiated with a loading page and will be updated
  // when quick answers result from server side is ready.
  quick_answers_view()->UpdateView(quick_answer);
}

void QuickAnswersUiController::SetActiveQuery(Profile* profile,
                                              const std::string& query) {
  profile_ = profile;
  query_ = query;
}

void QuickAnswersUiController::ShowRetry() {
  if (!IsShowingQuickAnswersView())
    return;

  quick_answers_view()->ShowRetryView();
}

void QuickAnswersUiController::CreateUserConsentView(
    const gfx::Rect& anchor_bounds,
    const std::u16string& intent_type,
    const std::u16string& intent_text) {
  CHECK_EQ(controller_->GetQuickAnswersVisibility(),
           QuickAnswersVisibility::kPending);

  auto* view = GetReadWriteCardsUiController().SetQuickAnswersUi(
      std::make_unique<quick_answers::UserConsentView>(
          anchor_bounds, intent_type, intent_text, weak_factory_.GetWeakPtr()));
  user_consent_view_.SetView(view);
}

void QuickAnswersUiController::CloseUserConsentView() {
  CHECK_EQ(controller_->GetQuickAnswersVisibility(),
           QuickAnswersVisibility::kUserConsentVisible);
  GetReadWriteCardsUiController().RemoveQuickAnswersUi();
}

void QuickAnswersUiController::OnSettingsButtonPressed() {
  // Route dismissal through |controller_| for logging impressions.
  controller_->DismissQuickAnswers(QuickAnswersExitPoint::kSettingsButtonClick);

  OpenSettings();
}

void QuickAnswersUiController::OpenSettings() {
  if (!fake_open_settings_callback_.is_null()) {
    CHECK_IS_TEST();
    fake_open_settings_callback_.Run();
    return;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  OpenUrl(profile_, GURL(kQuickAnswersSettingsUrl));
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  // OS settings app is implemented in Ash, but OpenUrl here does not qualify
  // for redirection in Lacros due to security limitations. Thus we need to
  // explicitly send the request to Ash to launch the OS settings app.
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  DCHECK(service->IsAvailable<crosapi::mojom::UrlHandler>());

  service->GetRemote<crosapi::mojom::UrlHandler>()->OpenUrl(
      GURL(kQuickAnswersSettingsUrl));
#endif
}

void QuickAnswersUiController::SetFakeOpenSettingsCallbackForTesting(
    QuickAnswersUiController::FakeOpenSettingsCallback
        fake_open_settings_callback) {
  CHECK_IS_TEST();
  CHECK(!fake_open_settings_callback.is_null());
  CHECK(fake_open_settings_callback_.is_null());
  fake_open_settings_callback_ = fake_open_settings_callback;
}

void QuickAnswersUiController::OnReportQueryButtonPressed() {
  controller_->DismissQuickAnswers(
      QuickAnswersExitPoint::kReportQueryButtonClick);

  OpenFeedbackPage(
      base::StringPrintf(kFeedbackDescriptionTemplate, query_.c_str()));
}

void QuickAnswersUiController::SetFakeOpenFeedbackPageCallbackForTesting(
    QuickAnswersUiController::FakeOpenFeedbackPageCallback
        fake_open_feedback_page_callback) {
  CHECK_IS_TEST();
  CHECK(!fake_open_feedback_page_callback.is_null());
  CHECK(fake_open_feedback_page_callback_.is_null());
  fake_open_feedback_page_callback_ = fake_open_feedback_page_callback;
}

void QuickAnswersUiController::OpenFeedbackPage(
    const std::string& feedback_template) {
  if (!fake_open_feedback_page_callback_.is_null()) {
    CHECK_IS_TEST();
    fake_open_feedback_page_callback_.Run(feedback_template);
    return;
  }

  // TODO(b/229007013): Merge the logics after resolve the deps cycle with
  // //c/b/ui in ash chrome build.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::NewWindowDelegate::GetPrimary()->OpenFeedbackPage(
      ash::NewWindowDelegate::FeedbackSource::kFeedbackSourceQuickAnswers,
      feedback_template);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  chrome::OpenFeedbackDialog(
      chrome::FindBrowserWithActiveWindow(),
      feedback::FeedbackSource::kFeedbackSourceQuickAnswers, feedback_template);
#endif
}

void QuickAnswersUiController::OnUserConsentResult(bool consented) {
  DCHECK(IsShowingUserConsentView());
  controller_->OnUserConsentResult(consented);

  if (consented && IsShowingQuickAnswersView())
    quick_answers_view()->RequestFocus();
}

bool QuickAnswersUiController::IsShowingUserConsentView() const {
  if (user_consent_view_) {
    CHECK_EQ(controller_->GetQuickAnswersVisibility(),
             QuickAnswersVisibility::kUserConsentVisible);
    return true;
  }

  return false;
}

bool QuickAnswersUiController::IsShowingQuickAnswersView() const {
  if (quick_answers_view_) {
    CHECK_EQ(controller_->GetQuickAnswersVisibility(),
             QuickAnswersVisibility::kQuickAnswersVisible);
    return true;
  }

  return false;
}

bool QuickAnswersUiController::IsShowingRichAnswersView() const {
  return rich_answers_widget_ && !rich_answers_widget_->IsClosed() &&
         rich_answers_widget_->GetContentsView();
}

chromeos::ReadWriteCardsUiController&
QuickAnswersUiController::GetReadWriteCardsUiController() const {
  return controller_->read_write_cards_ui_controller();
}
