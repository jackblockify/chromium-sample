// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/sync_confirmation_ui.h"

#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/profiles/profile_management_step_controller.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view_test_utils.h"
#include "chrome/browser/ui/views/profiles/profiles_pixel_test_utils.h"
#include "chrome/browser/ui/webui/signin/signin_url_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/widget/any_widget_observer.h"

#if !BUILDFLAG(ENABLE_DICE_SUPPORT) && !BUILDFLAG(IS_CHROMEOS_LACROS)
#error Platform not supported
#endif

// TODO(crbug.com/40242558): Move this file next to sync_confirmation_ui.cc.
// Render the page in a browser instead of a profile_picker_view to be able to
// do so.

// Tests for the chrome://sync-confirmation WebUI page. They live here and not
// in the webui directory because they manipulate views.
namespace {

// Configures the state of ::switches::kMinorModeRestrictionsForHistorySyncOptIn
// that relies on can_show_history_sync_opt_ins_without_minor_mode_restrictions
// capability.
struct MinorModeRestrictions {
  // Enable or disable the Feature
  bool enable_feature = false;
  // Related capability value
  signin::Tribool capability = signin::Tribool::kTrue;
};

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
constexpr MinorModeRestrictions kWithMinorModeRestrictionsWithUnrestrictedUser{
    .enable_feature = true,
    .capability = signin::Tribool::kTrue};
constexpr MinorModeRestrictions kWithMinorModeRestrictionsWithRestrictedUser{
    .enable_feature = true,
    .capability = signin::Tribool::kFalse};
#endif

void ConfigureMinorModeRestrictionFeature(
    MinorModeRestrictions minor_mode_restrictions,
    base::test::ScopedFeatureList& feature_flag_) {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
  if (minor_mode_restrictions.enable_feature) {
    feature_flag_.InitAndEnableFeature(
        ::switches::kMinorModeRestrictionsForHistorySyncOptIn);
  } else {
    feature_flag_.InitAndDisableFeature(
        ::switches::kMinorModeRestrictionsForHistorySyncOptIn);
  }
#else
  CHECK(!minor_mode_restrictions.enable_feature)
      << "This feature can be only enabled for selected platforms.";
#endif
}

struct SyncConfirmationTestParam {
  PixelTestParam pixel_test_param;
  AccountManagementStatus account_management_status =
      AccountManagementStatus::kNonManaged;
  SyncConfirmationStyle sync_style = SyncConfirmationStyle::kWindow;
  bool is_sync_promo = false;
  MinorModeRestrictions minor_mode_restrictions;
};

// To be passed as 4th argument to `INSTANTIATE_TEST_SUITE_P()`, allows the test
// to be named like `<TestClassName>.InvokeUi_default/<TestSuffix>` instead
// of using the index of the param in `TestParam` as suffix.
std::string ParamToTestSuffix(
    const ::testing::TestParamInfo<SyncConfirmationTestParam>& info) {
  return info.param.pixel_test_param.test_suffix;
}

// Permutations of supported parameters.
const SyncConfirmationTestParam kWindowTestParams[] = {
    {.pixel_test_param = {.test_suffix = "Regular"}},
    {.pixel_test_param = {.test_suffix = "DarkTheme", .use_dark_theme = true}},
    {.pixel_test_param = {.test_suffix = "Rtl",
                          .use_right_to_left_language = true}},
    {.pixel_test_param = {.test_suffix = "SmallWindow",
                          .use_small_window = true}},
    // TODO(crbug.com/336964850): this test is flaky on windows.
#if !BUILDFLAG(IS_WIN)
    {.pixel_test_param = {.test_suffix = "ManagedAccount"},
     .account_management_status = AccountManagementStatus::kManaged},
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
    // Restricted mode is only implemented for these platforms.
    {.pixel_test_param = {.test_suffix =
                              "RegularWithRestrictionsWithUnrestrictedUser"},
     .minor_mode_restrictions = kWithMinorModeRestrictionsWithUnrestrictedUser},
    {.pixel_test_param = {.test_suffix =
                              "RegularWithRestrictionsWithRestrictedUser"},
     .minor_mode_restrictions = kWithMinorModeRestrictionsWithRestrictedUser},
#endif

};

const SyncConfirmationTestParam kDialogTestParams[] = {
    {.pixel_test_param = {.test_suffix = "Regular"},
     .sync_style = SyncConfirmationStyle::kDefaultModal},
// The sign-in intercept feature isn't enabled on Lacros.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
    {.pixel_test_param = {.test_suffix = "SigninInterceptStyle"},
     .sync_style = SyncConfirmationStyle::kSigninInterceptModal,
     .is_sync_promo = true},
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)
    {.pixel_test_param = {.test_suffix = "DarkTheme", .use_dark_theme = true},
     .sync_style = SyncConfirmationStyle::kDefaultModal},
    {.pixel_test_param = {.test_suffix = "Rtl",
                          .use_right_to_left_language = true},
     .sync_style = SyncConfirmationStyle::kDefaultModal},
    {.pixel_test_param = {.test_suffix = "Promo"},
     .sync_style = SyncConfirmationStyle::kDefaultModal,
     .is_sync_promo = true},

// TODO(crbug.com/336964850): this test is flaky on windows.
#if !BUILDFLAG(IS_WIN)
    {.pixel_test_param = {.test_suffix = "ManagedAccount"},
     .account_management_status = AccountManagementStatus::kManaged,
     .sync_style = SyncConfirmationStyle::kDefaultModal},
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
    // Restricted mode is only implemented for these platforms.
    {.pixel_test_param = {.test_suffix =
                              "RegularWithRestrictionsWithUnrestrictedUser"},
     .sync_style = SyncConfirmationStyle::kDefaultModal,
     .minor_mode_restrictions = kWithMinorModeRestrictionsWithUnrestrictedUser},
    {.pixel_test_param = {.test_suffix =
                              "RegularWithRestrictionsWithRestrictedUser"},
     .sync_style = SyncConfirmationStyle::kDefaultModal,
     .minor_mode_restrictions = kWithMinorModeRestrictionsWithRestrictedUser},
#endif

};

GURL BuildSyncConfirmationWindowURL() {
  std::string url_string = chrome::kChromeUISyncConfirmationURL;
  return AppendSyncConfirmationQueryParams(GURL(url_string),
                                           SyncConfirmationStyle::kWindow,
                                           /*is_sync_promo=*/true);
}

// Creates a step to represent the sync-confirmation.
class SyncConfirmationStepControllerForTest
    : public ProfileManagementStepController {
 public:
  explicit SyncConfirmationStepControllerForTest(
      ProfilePickerWebContentsHost* host)
      : ProfileManagementStepController(host),
        sync_confirmation_url_(BuildSyncConfirmationWindowURL()) {}

  ~SyncConfirmationStepControllerForTest() override = default;

  void Show(StepSwitchFinishedCallback step_shown_callback,
            bool reset_state) override {
    // Reload the WebUI in the picker contents.
    host()->ShowScreenInPickerContents(
        sync_confirmation_url_,
        base::BindOnce(
            &SyncConfirmationStepControllerForTest::OnSyncConfirmationLoaded,
            weak_ptr_factory_.GetWeakPtr(), std::move(step_shown_callback)));
  }

  void OnNavigateBackRequested() override { NOTREACHED_NORETURN(); }

  void OnSyncConfirmationLoaded(
      StepSwitchFinishedCallback step_shown_callback) {
    SyncConfirmationUI* sync_confirmation_ui = static_cast<SyncConfirmationUI*>(
        host()->GetPickerContents()->GetWebUI()->GetController());

    sync_confirmation_ui->InitializeMessageHandlerWithBrowser(nullptr);

    if (step_shown_callback) {
      std::move(step_shown_callback).Run(/*success=*/true);
    }
  }

 private:
  const GURL sync_confirmation_url_;
  base::WeakPtrFactory<SyncConfirmationStepControllerForTest> weak_ptr_factory_{
      this};
};
}  // namespace

class SyncConfirmationUIWindowPixelTest
    : public ProfilesPixelTestBaseT<UiBrowserTest>,
      public testing::WithParamInterface<SyncConfirmationTestParam> {
 public:
  SyncConfirmationUIWindowPixelTest()
      : ProfilesPixelTestBaseT<UiBrowserTest>(GetParam().pixel_test_param) {
    DCHECK(GetParam().sync_style == SyncConfirmationStyle::kWindow);

    ConfigureMinorModeRestrictionFeature(GetParam().minor_mode_restrictions,
                                         scoped_feature_list);
  }

  void ShowUi(const std::string& name) override {
    ui::ScopedAnimationDurationScaleMode disable_animation(
        ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
    DCHECK(browser());

    SignInWithAccount(GetParam().account_management_status,
                      signin::ConsentLevel::kSignin,
                      GetParam().minor_mode_restrictions.capability);
    profile_picker_view_ = new ProfileManagementStepTestView(
        ProfilePicker::Params::ForFirstRun(browser()->profile()->GetPath(),
                                           base::DoNothing()),
        ProfileManagementFlowController::Step::kPostSignInFlow,
        /*step_controller_factory=*/
        base::BindRepeating([](ProfilePickerWebContentsHost* host) {
          return std::unique_ptr<ProfileManagementStepController>(
              new SyncConfirmationStepControllerForTest(host));
        }));
    profile_picker_view_->ShowAndWait(
        GetParam().pixel_test_param.use_small_window
            ? std::optional<gfx::Size>(gfx::Size(750, 590))
            : std::nullopt);
  }

  bool VerifyUi() override {
    views::Widget* widget = GetWidgetForScreenshot();

    auto* test_info = testing::UnitTest::GetInstance()->current_test_info();
    const std::string screenshot_name =
        base::StrCat({test_info->test_suite_name(), "_", test_info->name()});

    return VerifyPixelUi(widget, "SyncConfirmationUIWindowPixelTest",
                         screenshot_name) != ui::test::ActionResult::kFailed;
  }

  void WaitForUserDismissal() override {
    DCHECK(GetWidgetForScreenshot());
    ViewDeletedWaiter(profile_picker_view_).Wait();
  }

 private:
  views::Widget* GetWidgetForScreenshot() {
    return profile_picker_view_->GetWidget();
  }

  raw_ptr<ProfileManagementStepTestView, DanglingUntriaged>
      profile_picker_view_;

  base::test::ScopedFeatureList scoped_feature_list;
};

// TODO(crbug.com/40261456): Enable once `VerifyUi()` is non-flaky.
#if BUILDFLAG(IS_WIN)
#define MAYBE_InvokeUi_default DISABLED_InvokeUi_default
#else
#define MAYBE_InvokeUi_default InvokeUi_default
#endif  // BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_P(SyncConfirmationUIWindowPixelTest,
                       MAYBE_InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(,
                         SyncConfirmationUIWindowPixelTest,
                         testing::ValuesIn(kWindowTestParams),
                         &ParamToTestSuffix);

class SyncConfirmationUIDialogPixelTest
    : public ProfilesPixelTestBaseT<DialogBrowserTest>,
      public testing::WithParamInterface<SyncConfirmationTestParam> {
 public:
  SyncConfirmationUIDialogPixelTest()
      : ProfilesPixelTestBaseT<DialogBrowserTest>(GetParam().pixel_test_param) {
    DCHECK(GetParam().sync_style != SyncConfirmationStyle::kWindow);

    ConfigureMinorModeRestrictionFeature(GetParam().minor_mode_restrictions,
                                         scoped_feature_list);
  }

  ~SyncConfirmationUIDialogPixelTest() override = default;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    DCHECK(browser());

    SignInWithAccount(GetParam().account_management_status,
                      signin::ConsentLevel::kSignin,
                      GetParam().minor_mode_restrictions.capability);
    auto url = GURL(chrome::kChromeUISyncConfirmationURL);
    url = AppendSyncConfirmationQueryParams(url, GetParam().sync_style,
                                            GetParam().is_sync_promo);
    content::TestNavigationObserver observer(url);
    observer.StartWatchingNewWebContents();

    // ShowUi() can sometimes return before the dialog widget is shown because
    // the call to show the latter is asynchronous. Adding
    // NamedWidgetShownWaiter will prevent that from happening.
    views::NamedWidgetShownWaiter widget_waiter(
        views::test::AnyWidgetTestPasskey{},
        "SigninViewControllerDelegateViews");

    auto* controller = browser()->signin_view_controller();
    controller->ShowModalSyncConfirmationDialog(
        GetParam().sync_style == SyncConfirmationStyle::kSigninInterceptModal,
        GetParam().is_sync_promo);
    widget_waiter.WaitIfNeededAndGet();
    observer.Wait();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list;
};

IN_PROC_BROWSER_TEST_P(SyncConfirmationUIDialogPixelTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(,
                         SyncConfirmationUIDialogPixelTest,
                         testing::ValuesIn(kDialogTestParams),
                         &ParamToTestSuffix);
