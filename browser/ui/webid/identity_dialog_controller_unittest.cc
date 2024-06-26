// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webid/identity_dialog_controller.h"

#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ui/webid/account_selection_view.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/permissions/test/mock_permission_request.h"
#include "content/public/browser/identity_request_account.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

constexpr char kTopFrameEtldPlusOne[] = "top-frame-example.com";
constexpr char kIdpEtldPlusOne[] = "idp-example.com";

class IdentityDialogControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  IdentityDialogControllerTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~IdentityDialogControllerTest() override = default;
  IdentityDialogControllerTest(IdentityDialogControllerTest&) = delete;
  IdentityDialogControllerTest& operator=(IdentityDialogControllerTest&) =
      delete;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    SetContents(CreateTestWebContents());
    NavigateAndCommit(GURL(permissions::MockPermissionRequest::kDefaultOrigin));
    permissions::PermissionRequestManager::CreateForWebContents(web_contents());
  }

  void TearDown() override { ChromeRenderViewHostTestHarness::TearDown(); }

  void WaitForBubbleToBeShown(permissions::PermissionRequestManager* manager) {
    manager->DocumentOnLoadCompletedInPrimaryMainFrame();
    task_environment()->RunUntilIdle();
  }

  void Accept(permissions::PermissionRequestManager* manager) {
    manager->Accept();
    task_environment()->RunUntilIdle();
  }

  void Deny(permissions::PermissionRequestManager* manager) {
    manager->Deny();
    task_environment()->RunUntilIdle();
  }

  void Dismiss(permissions::PermissionRequestManager* manager) {
    manager->Dismiss();
    task_environment()->RunUntilIdle();
  }

  std::vector<content::IdentityRequestAccount> CreateAccount() {
    return {
        {"account_id1", "", "", "", GURL(),
         /*login_hints=*/std::vector<std::string>(),
         /*domain_hints=*/std::vector<std::string>(),
         /*labels=*/std::vector<std::string>(),
         /*login_state=*/content::IdentityRequestAccount::LoginState::kSignUp,
         /*browser_trusted_login_state=*/
         content::IdentityRequestAccount::LoginState::kSignUp}};
  }

  content::IdentityProviderData CreateIdentityProviderData(
      std::vector<content::IdentityRequestAccount> accounts) {
    return {kIdpEtldPlusOne,
            accounts,
            content::IdentityProviderMetadata(),
            content::ClientMetadata(GURL(), GURL(), GURL()),
            blink::mojom::RpContext::kSignIn,
            /*request_permission=*/true,
            /*has_login_status_mismatch=*/false};
  }
};

// Mock version of AccountSelectionView for injection during tests.
class MockAccountSelectionView : public AccountSelectionView {
 public:
  MockAccountSelectionView() : AccountSelectionView(/*delegate=*/nullptr) {}
  ~MockAccountSelectionView() override = default;

  MockAccountSelectionView(const MockAccountSelectionView&) = delete;
  MockAccountSelectionView& operator=(const MockAccountSelectionView&) = delete;

  MOCK_METHOD(
      bool,
      Show,
      (const std::string& top_frame_for_display,
       const std::optional<std::string>& iframe_for_display,
       const std::vector<content::IdentityProviderData>& identity_provider_data,
       Account::SignInMode sign_in_mode,
       blink::mojom::RpMode rp_mode,
       const std::optional<content::IdentityProviderData>& new_account_idp),
      (override));

  MOCK_METHOD(bool,
              ShowFailureDialog,
              (const std::string& top_frame_for_display,
               const std::optional<std::string>& iframe_for_display,
               const std::string& idp_for_display,
               blink::mojom::RpContext rp_context,
               blink::mojom::RpMode rp_mode,
               const content::IdentityProviderMetadata& idp_metadata),
              (override));

  MOCK_METHOD(bool,
              ShowErrorDialog,
              (const std::string& top_frame_for_display,
               const std::optional<std::string>& iframe_for_display,
               const std::string& idp_for_display,
               blink::mojom::RpContext rp_context,
               blink::mojom::RpMode rp_mode,
               const content::IdentityProviderMetadata& idp_metadata,
               const std::optional<TokenError>& error),
              (override));

  MOCK_METHOD(bool,
              ShowLoadingDialog,
              (const std::string& top_frame_for_display,
               const std::string& idp_for_display,
               blink::mojom::RpContext rp_context,
               blink::mojom::RpMode rp_mode),
              (override));

  MOCK_METHOD(std::string, GetTitle, (), (const, override));

  MOCK_METHOD(std::optional<std::string>, GetSubtitle, (), (const, override));

  MOCK_METHOD(void, ShowUrl, (LinkType type, const GURL& url), (override));

  MOCK_METHOD(content::WebContents*,
              ShowModalDialog,
              (const GURL& url),
              (override));

  MOCK_METHOD(void, CloseModalDialog, (), (override));
};

TEST_F(IdentityDialogControllerTest, Accept) {
  IdentityDialogController controller(web_contents());

  base::MockCallback<base::OnceCallback<void(bool accepted)>> callback;
  EXPECT_CALL(callback, Run(true)).WillOnce(testing::Return());
  controller.RequestIdPRegistrationPermision(
      url::Origin::Create(GURL("https://idp.example")), callback.Get());

  auto* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents());

  auto prompt_factory =
      std::make_unique<permissions::MockPermissionPromptFactory>(manager);

  WaitForBubbleToBeShown(manager);

  EXPECT_TRUE(prompt_factory->is_visible());

  Accept(manager);

  EXPECT_FALSE(prompt_factory->is_visible());
}

TEST_F(IdentityDialogControllerTest, Deny) {
  IdentityDialogController controller(web_contents());

  base::MockCallback<base::OnceCallback<void(bool accepted)>> callback;
  EXPECT_CALL(callback, Run(false)).WillOnce(testing::Return());
  controller.RequestIdPRegistrationPermision(
      url::Origin::Create(GURL("https://idp.example")), callback.Get());

  auto* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents());

  auto prompt_factory =
      std::make_unique<permissions::MockPermissionPromptFactory>(manager);

  WaitForBubbleToBeShown(manager);

  EXPECT_TRUE(prompt_factory->is_visible());

  Deny(manager);

  EXPECT_FALSE(prompt_factory->is_visible());
}

TEST_F(IdentityDialogControllerTest, Dismiss) {
  IdentityDialogController controller(web_contents());

  base::MockCallback<base::OnceCallback<void(bool accepted)>> callback;
  EXPECT_CALL(callback, Run(false)).WillOnce(testing::Return());
  controller.RequestIdPRegistrationPermision(
      url::Origin::Create(GURL("https://idp.example")), callback.Get());

  auto* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents());

  auto prompt_factory =
      std::make_unique<permissions::MockPermissionPromptFactory>(manager);

  WaitForBubbleToBeShown(manager);

  EXPECT_TRUE(prompt_factory->is_visible());

  Dismiss(manager);

  EXPECT_FALSE(prompt_factory->is_visible());
}

// Test that selecting an account in button mode, and then dismissing it should
// run the dismiss callback.
TEST_F(IdentityDialogControllerTest, OnAccountSelectedButtonCallsDismiss) {
  IdentityDialogController controller(web_contents());
  controller.SetAccountSelectionViewForTesting(
      std::make_unique<MockAccountSelectionView>());

  std::vector<content::IdentityRequestAccount> accounts = CreateAccount();
  content::IdentityProviderData idp_data = CreateIdentityProviderData(accounts);

  // Dismiss callback should run once.
  base::MockCallback<DismissCallback> dismiss_callback;
  EXPECT_CALL(dismiss_callback, Run).WillOnce(testing::Return());

  // Show button mode accounts dialog.
  controller.ShowAccountsDialog(
      kTopFrameEtldPlusOne, /*iframe_for_display=*/std::nullopt, {idp_data},
      content::IdentityRequestAccount::SignInMode::kExplicit,
      blink::mojom::RpMode::kButton, /*new_account_idp=*/std::nullopt,
      /*on_selected=*/base::DoNothing(), /*on_add_account=*/base::DoNothing(),
      /*dismiss_callback=*/dismiss_callback.Get(),
      /*accounts_displayed_callback=*/base::DoNothing());

  // User selects an account, and then dismisses it. The expectation set for
  // dismiss callback should pass.
  controller.OnAccountSelected(GURL(kIdpEtldPlusOne), accounts[0]);
  controller.OnDismiss(IdentityDialogController::DismissReason::kOther);
}

// Test that selecting an account in widget, and then dismissing it should not
// run the dismiss callback.
TEST_F(IdentityDialogControllerTest, OnAccountSelectedWidgetResetsDismiss) {
  IdentityDialogController controller(web_contents());
  controller.SetAccountSelectionViewForTesting(
      std::make_unique<MockAccountSelectionView>());

  std::vector<content::IdentityRequestAccount> accounts = CreateAccount();
  content::IdentityProviderData idp_data = CreateIdentityProviderData(accounts);

  // Dismiss callback should not be run.
  base::MockCallback<DismissCallback> dismiss_callback;
  EXPECT_CALL(dismiss_callback, Run).Times(0);

  // Show widget mode accounts dialog.
  controller.ShowAccountsDialog(
      kTopFrameEtldPlusOne, /*iframe_for_display=*/std::nullopt, {idp_data},
      content::IdentityRequestAccount::SignInMode::kExplicit,
      blink::mojom::RpMode::kWidget, /*new_account_idp=*/std::nullopt,
      /*on_selected=*/base::DoNothing(), /*on_add_account=*/base::DoNothing(),
      /*dismiss_callback=*/dismiss_callback.Get(),
      /*accounts_displayed_callback=*/base::DoNothing());

  // User selects an account, and then dismisses it. The expectation set for
  // dismiss callback should pass.
  controller.OnAccountSelected(GURL(kIdpEtldPlusOne), accounts[0]);
  controller.OnDismiss(IdentityDialogController::DismissReason::kOther);
}