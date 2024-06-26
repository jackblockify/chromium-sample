// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/shimless_rma/chrome_shimless_rma_delegate.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/shimless_rma/diagnostics_app_profile_helper.h"
#include "chrome/browser/ash/shimless_rma/diagnostics_app_profile_helper_constants.h"
#include "chrome/browser/extensions/extension_garbage_collector_factory.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_storage_location.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/fake_service_worker_context.h"
#include "extensions/common/constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy_declaration.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"

namespace ash::shimless_rma {
namespace {

const char kTestCrxPath[] = "chrome/test/data/chromeos/3p_diagnostics/diag.crx";
const char kTestWrongIdCrxPath[] =
    "chrome/test/data/chromeos/3p_diagnostics/diag-wrong-id.crx";
// The test wrong id generated by the key signing the above crx.
const char kTestWrongExtId[] = "neacocmolncbbnnameegalgmoedgpfpk";
// The IWA installation is not tested in unit test. So we don't need a real
// IWA.
const char kFakeIwaPath[] = "fake_iwa_path.swbn";
// The IWA ID corresponding to the dev extension, used in development phase.
const char kDevIwaId[] =
    "pt2jysa7yu326m2cbu5mce4rrajvguagronrsqwn5dhbaris6eaaaaic";
}  // namespace

class ChromeShimlessRmaDelegateTest : public testing::Test {
 public:
  ChromeShimlessRmaDelegateTest()
      : chrome_shimless_rma_delegate_(ChromeShimlessRmaDelegate(nullptr)),
        task_environment_(content::BrowserTaskEnvironment::REAL_IO_THREAD) {}
  ~ChromeShimlessRmaDelegateTest() override = default;

 protected:
  ChromeShimlessRmaDelegate chrome_shimless_rma_delegate_;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

// Validates a QrCode Bitmap is correctly converted to a string.
TEST_F(ChromeShimlessRmaDelegateTest, GenerateQrCode) {
  base::RunLoop run_loop;
  chrome_shimless_rma_delegate_.GenerateQrCode(
      "www.sample-url.com",
      base::BindLambdaForTesting([&](const std::string& qr_code_image) {
        EXPECT_FALSE(qr_code_image.empty());
      }));
  run_loop.RunUntilIdle();
}

class FakeServiceWorkerContext : public content::FakeServiceWorkerContext {
 public:
  FakeServiceWorkerContext() = default;
  FakeServiceWorkerContext(const FakeServiceWorkerContext&) = delete;
  ~FakeServiceWorkerContext() override = default;

  void CheckHasServiceWorker(const GURL& url,
                             const blink::StorageKey& key,
                             CheckHasServiceWorkerCallback callback) override {
    content::ServiceWorkerCapability result =
        content::ServiceWorkerCapability::SERVICE_WORKER_NO_FETCH_HANDLER;
    if (service_worker_check_retry_ > 0) {
      --service_worker_check_retry_;
      result = content::ServiceWorkerCapability::NO_SERVICE_WORKER;
    }
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), result));
  }

  void set_service_worker_check_retry(int64_t retry) {
    service_worker_check_retry_ = retry;
  }

 private:
  // The times until the check return service worker found.
  int64_t service_worker_check_retry_ = 2;
};

class FakeWebAppCommandScheduler : public web_app::WebAppCommandScheduler {
 public:
  using web_app::WebAppCommandScheduler::WebAppCommandScheduler;

  void InstallIsolatedWebApp(
      const web_app::IsolatedWebAppUrlInfo& url_info,
      const web_app::IsolatedWebAppInstallSource& install_source,
      const std::optional<base::Version>& expected_version,
      std::unique_ptr<ScopedKeepAlive> keep_alive,
      std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive,
      web_app::WebAppCommandScheduler::InstallIsolatedWebAppCallback callback,
      const base::Location& call_location) override {
    EXPECT_EQ(install_source.install_surface(),
              webapps::WebappInstallSource::IWA_SHIMLESS_RMA);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            web_app::InstallIsolatedWebAppCommandSuccess(
                base::Version{}, web_app::IwaStorageOwnedBundle{
                                     "random_folder", /*dev_mode=*/false})));
  }
};

class FakeDiagnosticsAppProfileHelperDelegate
    : public DiagnosticsAppProfileHelperDelegate {
 public:
  explicit FakeDiagnosticsAppProfileHelperDelegate(Profile* profile)
      : web_app_command_scheduler_(*profile) {}
  FakeDiagnosticsAppProfileHelperDelegate(
      const DiagnosticsAppProfileHelperDelegate&) = delete;
  ~FakeDiagnosticsAppProfileHelperDelegate() override = default;

  content::ServiceWorkerContext* GetServiceWorkerContextForExtensionId(
      const extensions::ExtensionId& extension_id,
      content::BrowserContext* browser_context) override {
    return &fake_service_worker_context_;
  }

  web_app::WebAppCommandScheduler* GetWebAppCommandScheduler(
      content::BrowserContext* browser_context) override {
    return &web_app_command_scheduler_;
  }

  const web_app::WebApp* GetWebAppById(
      const webapps::AppId& app_id,
      content::BrowserContext* browser_context) override {
    return &web_app_;
  }

  FakeServiceWorkerContext& fake_service_worker_context() {
    return fake_service_worker_context_;
  }

  web_app::WebApp& web_app() { return web_app_; }

 protected:
  FakeServiceWorkerContext fake_service_worker_context_;
  FakeWebAppCommandScheduler web_app_command_scheduler_;
  web_app::WebApp web_app_{/*AppId=*/""};
};

class ChromeShimlessRmaDelegatePrepareDiagnosticsAppProfileTest
    : public extensions::ExtensionServiceTestBase {
 public:
  ChromeShimlessRmaDelegatePrepareDiagnosticsAppProfileTest()
      : extensions::ExtensionServiceTestBase(
            std::make_unique<content::BrowserTaskEnvironment>(
                base::test::TaskEnvironment::TimeSource::MOCK_TIME)) {}

  void SetUp() override {
    extensions::ExtensionServiceTestBase::SetUp();

    feature_list_.InitWithFeatures(
        {
            ash::features::kShimlessRMA3pDiagnostics,
            ash::features::kShimlessRMA3pDiagnosticsDevMode,
            ash::features::kShimlessRMA3pDiagnosticsAllowPermissionPolicy,
        },
        {});
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    TestingProfile* profile = testing_profile_manager_.CreateTestingProfile(
        kShimlessRmaAppBrowserContextBaseName);

    fake_diagnostics_app_profile_helper_delegate_ =
        std::make_unique<FakeDiagnosticsAppProfileHelperDelegate>(profile);

    InitializeExtensionSystem(profile);

    chrome_shimless_rma_delegate_
        .SetDiagnosticsAppProfileHelperDelegateForTesting(
            fake_diagnostics_app_profile_helper_delegate_.get());
  }

  void InitializeExtensionSystem(Profile* profile) {
    auto extensions_install_dir =
        profile->GetPath().AppendASCII(extensions::kInstallDirectoryName);
    auto unpacked_install_dir = profile->GetPath().AppendASCII(
        extensions::kUnpackedInstallDirectoryName);

    extensions::TestExtensionSystem* system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile));
    auto* service = system->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(), extensions_install_dir,
        unpacked_install_dir, true, true);

    // When we start up, we want to make sure there is no external provider,
    // since the ExtensionService on Windows will use the Registry as a default
    // provider and if there is something already registered there then it will
    // interfere with the tests. Those tests that need an external provider
    // will register one specifically.
    service->ClearProvidersForTesting();

    service->Init();

    // Garbage collector is typically NULL during tests, so give it a build.
    extensions::ExtensionGarbageCollectorFactory::GetInstance()
        ->SetTestingFactoryAndUse(
            profile,
            base::BindRepeating(&extensions::ExtensionGarbageCollectorFactory::
                                    BuildInstanceFor));
  }

  void TearDown() override { extensions::ExtensionServiceTestBase::TearDown(); }

  using Result = base::expected<
      ChromeShimlessRmaDelegate::PrepareDiagnosticsAppBrowserContextResult,
      std::string>;
  Result PrepareDiagnosticsAppBrowserContext(const base::FilePath& crx_path) {
    base::test::TestFuture<Result> future;
    chrome_shimless_rma_delegate_.PrepareDiagnosticsAppBrowserContext(
        crx_path, base::FilePath{kFakeIwaPath}, future.GetCallback());
    return future.Get();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  TestingProfileManager testing_profile_manager_{
      TestingBrowserProcess::GetGlobal(), &testing_local_state_};
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  std::unique_ptr<FakeDiagnosticsAppProfileHelperDelegate>
      fake_diagnostics_app_profile_helper_delegate_;
  ChromeShimlessRmaDelegate chrome_shimless_rma_delegate_{nullptr};
};

// Verify the whole flow of `PrepareDiagnosticsAppProfile`.
TEST_F(ChromeShimlessRmaDelegatePrepareDiagnosticsAppProfileTest, Success) {
  const auto expected_url_origin =
      url::Origin::Create(GURL(base::StrCat({"isolated-app://", kDevIwaId})));
  fake_diagnostics_app_profile_helper_delegate_->web_app().SetName("App Name");
  fake_diagnostics_app_profile_helper_delegate_->web_app().SetStartUrl(
      expected_url_origin.GetURL());

  // Call this twice to verify that even if the profile has already been loaded
  // it still works.
  for (int i = 0; i < 2; ++i) {
    auto result = PrepareDiagnosticsAppBrowserContext(
        base::PathService::CheckedGet(base::DIR_SRC_TEST_DATA_ROOT)
            .Append(kTestCrxPath));

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->extension_id, "jmalcmbicpnakfkncbgbcmlmgpfkhdca");
    EXPECT_EQ(result->iwa_id.id(), kDevIwaId);
    EXPECT_EQ(result->name, "App Name");
    EXPECT_EQ(result->permission_message,
              "Run ChromeOS diagnostic tests\nRead ChromeOS device information "
              "and data\nRead ChromeOS device and component serial numbers\n");

    content::BrowserContext* context = result->context;
    EXPECT_FALSE(context->IsOffTheRecord());
    EXPECT_TRUE(Profile::FromBrowserContext(context)->GetPrefs()->GetBoolean(
        prefs::kForceEphemeralProfiles));
    EXPECT_TRUE(
        DiagnosticsAppProfileHelperDelegate::GetInstalledDiagnosticsAppOrigin()
            .has_value());
  }
}

// Verify that we denied extensions which is not in the ChromeOS system
// extension allowlist.
TEST_F(ChromeShimlessRmaDelegatePrepareDiagnosticsAppProfileTest,
       NotChromeOSSystemExtension) {
  auto result = PrepareDiagnosticsAppBrowserContext(
      base::PathService::CheckedGet(base::DIR_SRC_TEST_DATA_ROOT)
          .Append(kTestWrongIdCrxPath));

  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            base::StringPrintf(k3pDiagErrorNotChromeOSSystemExtension,
                               kTestWrongExtId));
}

// Verify the service worker polling logic break after reaching the timeout.
TEST_F(ChromeShimlessRmaDelegatePrepareDiagnosticsAppProfileTest,
       ServiceWorkerTimeout) {
  // To hit the timeout, we need to retry more than ((timeout / polling
  // interval) + 1) times.
  int64_t retry_times = k3pDiagExtensionReadyPollingTimeout.IntDiv(
                            k3pDiagExtensionReadyPollingInterval) +
                        1;
  fake_diagnostics_app_profile_helper_delegate_->fake_service_worker_context()
      .set_service_worker_check_retry(retry_times);

  auto result = PrepareDiagnosticsAppBrowserContext(
      base::PathService::CheckedGet(base::DIR_SRC_TEST_DATA_ROOT)
          .Append(kTestCrxPath));

  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), k3pDiagErrorCannotActivateExtension);
}

// Verify that IWA with allowlisted permission policy will be installed.
TEST_F(ChromeShimlessRmaDelegatePrepareDiagnosticsAppProfileTest,
       IWACanHaveAllowlistedPermissionsPolicy) {
  fake_diagnostics_app_profile_helper_delegate_->web_app().SetPermissionsPolicy(
      blink::ParsedPermissionsPolicy{
          {blink::ParsedPermissionsPolicyDeclaration{
               blink::mojom::PermissionsPolicyFeature::kCamera},
           blink::ParsedPermissionsPolicyDeclaration{
               blink::mojom::PermissionsPolicyFeature::kFullscreen},
           blink::ParsedPermissionsPolicyDeclaration{
               blink::mojom::PermissionsPolicyFeature::kMicrophone},
           blink::ParsedPermissionsPolicyDeclaration{
               blink::mojom::PermissionsPolicyFeature::kHid}}});

  auto result = PrepareDiagnosticsAppBrowserContext(
      base::PathService::CheckedGet(base::DIR_SRC_TEST_DATA_ROOT)
          .Append(kTestCrxPath));

  EXPECT_TRUE(result.has_value());
}

// Verify that IWA with not-allowlisted permission policy will be blocked.
TEST_F(ChromeShimlessRmaDelegatePrepareDiagnosticsAppProfileTest,
       IWACannotHavePermissionsPolicyOutsideAllowlist) {
  fake_diagnostics_app_profile_helper_delegate_->web_app().SetPermissionsPolicy(
      blink::ParsedPermissionsPolicy{
          blink::ParsedPermissionsPolicyDeclaration{
              blink::mojom::PermissionsPolicyFeature::kCamera},
          {blink::ParsedPermissionsPolicyDeclaration{
              blink::mojom::PermissionsPolicyFeature::kNotFound}}});

  auto result = PrepareDiagnosticsAppBrowserContext(
      base::PathService::CheckedGet(base::DIR_SRC_TEST_DATA_ROOT)
          .Append(kTestCrxPath));

  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), k3pDiagErrorIWACannotHasPermissionPolicy);
}

// Verify that IWA with allowlisted permission policy but without feature flag
// will be blocked.
TEST_F(ChromeShimlessRmaDelegatePrepareDiagnosticsAppProfileTest,
       IWACannotHavePermissionsPolicyWithoutFeatureFlag) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndDisableFeature(
      ash::features::kShimlessRMA3pDiagnosticsAllowPermissionPolicy);

  fake_diagnostics_app_profile_helper_delegate_->web_app().SetPermissionsPolicy(
      blink::ParsedPermissionsPolicy{{blink::ParsedPermissionsPolicyDeclaration{
          blink::mojom::PermissionsPolicyFeature::kCamera}}});

  auto result = PrepareDiagnosticsAppBrowserContext(
      base::PathService::CheckedGet(base::DIR_SRC_TEST_DATA_ROOT)
          .Append(kTestCrxPath));

  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), k3pDiagErrorIWACannotHasPermissionPolicy);
}

// Verify that if IWA is not installed successfully, the Delegate will not
// return the installed app origin.
TEST_F(ChromeShimlessRmaDelegatePrepareDiagnosticsAppProfileTest,
       InstalledAppOriginNotSetAfterIwaInstallFailure) {
  const auto expected_url_origin =
      url::Origin::Create(GURL(base::StrCat({"isolated-app://", kDevIwaId})));
  fake_diagnostics_app_profile_helper_delegate_->web_app().SetStartUrl(
      expected_url_origin.GetURL());

  fake_diagnostics_app_profile_helper_delegate_->web_app().SetPermissionsPolicy(
      blink::ParsedPermissionsPolicy{{blink::ParsedPermissionsPolicyDeclaration{
          blink::mojom::PermissionsPolicyFeature::kNotFound}}});

  auto result = PrepareDiagnosticsAppBrowserContext(
      base::PathService::CheckedGet(base::DIR_SRC_TEST_DATA_ROOT)
          .Append(kTestCrxPath));

  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), k3pDiagErrorIWACannotHasPermissionPolicy);
  EXPECT_FALSE(
      DiagnosticsAppProfileHelperDelegate::GetInstalledDiagnosticsAppOrigin()
          .has_value());
}

}  // namespace ash::shimless_rma
