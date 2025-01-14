// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/pdf_ocr_controller.h"

#include <vector>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "chrome/browser/accessibility/pdf_ocr_controller_factory.h"
#include "chrome/browser/pdf/pdf_extension_test_base.h"
#include "chrome/browser/screen_ai/screen_ai_install_state.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "pdf/pdf_features.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/accessibility_switches.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include <optional>

#include "chrome/browser/browser_process.h"
#include "content/public/test/scoped_accessibility_mode_override.h"
#else
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

class WebContentsLoadWaiter : public content::WebContentsObserver {
 public:
  // Observe `DidFinishLoad` for the specified |web_contents|.
  explicit WebContentsLoadWaiter(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}

  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override {
    run_loop_.Quit();
  }

  void Wait() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_{base::RunLoop::Type::kNestableTasksAllowed};
};

class DownloadObserver : public screen_ai::ScreenAIInstallState::Observer {
 public:
  DownloadObserver() {
    install_state_observer_.Observe(
        screen_ai::ScreenAIInstallState::GetInstance());
  }

  DownloadObserver(const DownloadObserver&) = delete;
  DownloadObserver& operator=(const DownloadObserver&) = delete;

  ~DownloadObserver() override = default;

  // screen_ai::ScreenAIInstallState::Observer:
  void StateChanged(screen_ai::ScreenAIInstallState::State state) override {
    if (state != screen_ai::ScreenAIInstallState::State::kDownloading) {
      return;
    }

    screen_ai::ScreenAIInstallState::GetInstance()->SetState(
        screen_ai::ScreenAIInstallState::State::kDownloadFailed);

    remaining_download_tries_--;
    if (!remaining_download_tries_) {
      run_loop.Quit();
    }
  }

  void WaitForDownloads() {
    if (remaining_download_tries_) {
      run_loop.Run();
    }
  }

  int remaining_download_tries_ = -1;

 private:
  base::RunLoop run_loop;
  base::ScopedObservation<screen_ai::ScreenAIInstallState,
                          screen_ai::ScreenAIInstallState::Observer>
      install_state_observer_{this};
};

}  // namespace

class PdfOcrControllerBrowserTest : public base::test::WithFeatureOverride,
                                    public PDFExtensionTestBase {
 public:
  PdfOcrControllerBrowserTest()
      : base::test::WithFeatureOverride(chrome_pdf::features::kPdfOopif) {}
  ~PdfOcrControllerBrowserTest() override = default;

  PdfOcrControllerBrowserTest(const PdfOcrControllerBrowserTest&) = delete;
  PdfOcrControllerBrowserTest& operator=(const PdfOcrControllerBrowserTest&) =
      delete;

  // PDFExtensionTestBase overrides:
  void TearDownOnMainThread() override {
    PDFExtensionTestBase::TearDownOnMainThread();
    EnableScreenReader(false);
  }

  void EnableScreenReader(bool enabled) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Enable Chromevox.
    ash::AccessibilityManager::Get()->EnableSpokenFeedback(enabled);
#else
    if (!enabled) {
      scoped_accessibility_override_.reset();
    } else if (!scoped_accessibility_override_) {
      scoped_accessibility_override_.emplace(ui::AXMode::kScreenReader);
    }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void EnableSelectToSpeak(bool enabled) {
    ash::AccessibilityManager::Get()->SetSelectToSpeakEnabled(enabled);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  bool UseOopif() const override { return GetParam(); }

  std::vector<base::test::FeatureRef> GetEnabledFeatures() const override {
    auto enabled = PDFExtensionTestBase::GetEnabledFeatures();
    enabled.push_back(features::kPdfOcr);
#if BUILDFLAG(IS_CHROMEOS)
    enabled.push_back(features::kAccessibilityPdfOcrForSelectToSpeak);
#endif  // BUILDFLAG(IS_CHROMEOS)
    return enabled;
  }

 private:
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  std::optional<content::ScopedAccessibilityModeOverride>
      scoped_accessibility_override_;
#endif
};

// Enabling screen reader should affect the accessibility mode of a new
// WebContents of PDF Viewer Mimehandler.
IN_PROC_BROWSER_TEST_P(PdfOcrControllerBrowserTest,
                       OpenPDFAfterTurningOnScreenReader) {
  // Forced accessibility contradicts with turning off the screen reader.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceRendererAccessibility)) {
    GTEST_SKIP();
  }

  screen_ai::PdfOcrControllerFactory::GetForProfile(browser()->profile())
      ->set_ocr_ready_for_testing();
  ui::AXMode ax_mode =
      content::BrowserAccessibilityState::GetInstance()->GetAccessibilityMode();
  EXPECT_FALSE(ax_mode.has_mode(ui::AXMode::kPDFOcr));

  EnableScreenReader(true);
  // Load test PDF.
  content::WebContents* active_web_contents = GetActiveWebContents();
  WebContentsLoadWaiter load_waiter(active_web_contents);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/pdf/test.pdf")));
  load_waiter.Wait();

  std::vector<content::WebContents*> html_web_contents_vector =
      screen_ai::PdfOcrController::GetAllPdfWebContentsesForTesting(
          browser()->profile());
  for (auto* web_contents : html_web_contents_vector) {
    ax_mode = web_contents->GetAccessibilityMode();
    EXPECT_TRUE(ax_mode.has_mode(ui::AXMode::kPDFOcr));
  }
}

// Enabling screen reader should affect the accessibility mode of an exiting
// WebContents of PDF Viewer Mimehandler.
IN_PROC_BROWSER_TEST_P(PdfOcrControllerBrowserTest,
                       OpenPDFBeforeTurningOnScreenReader) {
  // Forced accessibility contradicts with turning off the screen reader.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceRendererAccessibility)) {
    GTEST_SKIP();
  }
  ui::AXMode ax_mode =
      content::BrowserAccessibilityState::GetInstance()->GetAccessibilityMode();
  EXPECT_FALSE(ax_mode.has_mode(ui::AXMode::kPDFOcr));

  // Load test PDF.
  content::WebContents* active_web_contents = GetActiveWebContents();
  WebContentsLoadWaiter load_waiter(active_web_contents);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/pdf/test.pdf")));
  load_waiter.Wait();

  std::vector<content::WebContents*> html_web_contents_vector =
      screen_ai::PdfOcrController::GetAllPdfWebContentsesForTesting(
          browser()->profile());
  for (auto* web_contents : html_web_contents_vector) {
    ax_mode = web_contents->GetAccessibilityMode();
    EXPECT_FALSE(ax_mode.has_mode(ui::AXMode::kPDFOcr));
  }

  screen_ai::PdfOcrControllerFactory::GetForProfile(browser()->profile())
      ->set_ocr_ready_for_testing();
  EnableScreenReader(true);

  html_web_contents_vector =
      screen_ai::PdfOcrController::GetAllPdfWebContentsesForTesting(
          browser()->profile());
  for (auto* web_contents : html_web_contents_vector) {
    ax_mode = web_contents->GetAccessibilityMode();
    EXPECT_TRUE(ax_mode.has_mode(ui::AXMode::kPDFOcr));
  }
}

IN_PROC_BROWSER_TEST_P(PdfOcrControllerBrowserTest, WithoutScreenReader) {
  // Forced accessibility contradicts with turning off the screen reader.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceRendererAccessibility)) {
    GTEST_SKIP();
  }

  EnableScreenReader(false);

  screen_ai::PdfOcrControllerFactory::GetForProfile(browser()->profile())
      ->set_ocr_ready_for_testing();
  screen_ai::PdfOcrControllerFactory::GetForProfile(browser()->profile())
      ->Activate();

  ASSERT_TRUE(LoadPdf(embedded_test_server()->GetURL("/pdf/test.pdf")));
  content::WebContents* pdf_contents = GetActiveWebContents();
  ui::AXMode ax_mode = pdf_contents->GetAccessibilityMode();
  EXPECT_FALSE(ax_mode.has_mode(ui::AXMode::kPDFOcr));
}

// Lacros does not download the library.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
// Retry download if it fails.
IN_PROC_BROWSER_TEST_P(PdfOcrControllerBrowserTest, DownloadRetry) {
  // Forced accessibility affects counting.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceRendererAccessibility)) {
    GTEST_SKIP();
  }

  screen_ai::PdfOcrControllerFactory::GetForProfile(browser()->profile())
      ->set_initialization_retry_wait_for_testing(base::Milliseconds(1));

  DownloadObserver observer;
  observer.remaining_download_tries_ = 3;

  EnableScreenReader(true);

  observer.WaitForDownloads();
}
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_P(PdfOcrControllerBrowserTest, WithoutSelectToSpeak) {
  EnableSelectToSpeak(false);

  screen_ai::PdfOcrControllerFactory::GetForProfile(browser()->profile())
      ->set_ocr_ready_for_testing();
  screen_ai::PdfOcrControllerFactory::GetForProfile(browser()->profile())
      ->Activate();

  ASSERT_TRUE(LoadPdf(embedded_test_server()->GetURL("/pdf/test.pdf")));
  content::WebContents* pdf_contents = GetActiveWebContents();
  ui::AXMode ax_mode = pdf_contents->GetAccessibilityMode();
  EXPECT_FALSE(ax_mode.has_mode(ui::AXMode::kPDFOcr));
}

IN_PROC_BROWSER_TEST_P(PdfOcrControllerBrowserTest, WithSelectToSpeak) {
  EnableSelectToSpeak(true);

  screen_ai::PdfOcrControllerFactory::GetForProfile(browser()->profile())
      ->set_ocr_ready_for_testing();
  screen_ai::PdfOcrControllerFactory::GetForProfile(browser()->profile())
      ->Activate();

  ASSERT_TRUE(LoadPdf(embedded_test_server()->GetURL("/pdf/test.pdf")));
  content::WebContents* pdf_contents = GetActiveWebContents();
  ui::AXMode ax_mode = pdf_contents->GetAccessibilityMode();
  EXPECT_TRUE(ax_mode.has_mode(ui::AXMode::kPDFOcr));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// TODO(crbug.com/40268279): Stop testing both modes after OOPIF PDF viewer
// launches.
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PdfOcrControllerBrowserTest);
