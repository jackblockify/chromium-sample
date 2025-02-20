// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble/webui_bubble_manager.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace {

void DestroySpareRenderProcess() {
  content::RenderProcessHost* spare_render_process_host =
      content::RenderProcessHost::GetSpareRenderProcessHostForTesting();
  if (!spare_render_process_host) {
    return;
  }

  content::RenderProcessHostWatcher kill_observer(
      spare_render_process_host,
      content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  spare_render_process_host->FastShutdownIfPossible(0);
  kill_observer.Wait();
}

// Close the bubble, clear any cached content wrapper, including the ones
// stored in the contents wrapper service.
void DestroyBubble(WebUIBubbleManager* bubble_manager, Profile* profile) {
  bubble_manager->CloseBubble();
  bubble_manager->ResetContentsWrapperForTesting();
  if (auto* service =
          WebUIContentsWrapperServiceFactory::GetForProfile(profile, true)) {
    service->Shutdown();
  }
  base::RunLoop().RunUntilIdle();
}

}  // namespace

class TestWebUIController : public TopChromeWebUIController {
  WEB_UI_CONTROLLER_TYPE_DECL();
};
WEB_UI_CONTROLLER_TYPE_IMPL(TestWebUIController)

template <>
class WebUIContentsWrapperT<TestWebUIController> final
    : public WebUIContentsWrapper {
 public:
  WebUIContentsWrapperT(const GURL& webui_url,
                        content::BrowserContext* browser_context,
                        int task_manager_string_id,
                        bool webui_resizes_host = true,
                        bool esc_closes_ui = true,
                        bool supports_draggable_regions = false)
      : WebUIContentsWrapper(webui_url,
                             browser_context,
                             task_manager_string_id,
                             webui_resizes_host,
                             esc_closes_ui,
                             supports_draggable_regions,
                             "Test") {}
  void ReloadWebContents() override {}
  base::WeakPtr<WebUIContentsWrapper> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<WebUIContentsWrapper> weak_ptr_factory_{this};
};

class WebUIBubbleManagerBrowserTest : public InProcessBrowserTest {
 public:
  WebUIBubbleManagerBrowserTest() = default;
  WebUIBubbleManagerBrowserTest(const WebUIBubbleManagerBrowserTest&) = delete;
  const WebUIBubbleManagerBrowserTest& operator=(
      const WebUIBubbleManagerBrowserTest&) = delete;
  ~WebUIBubbleManagerBrowserTest() override = default;

  // content::BrowserTestBase:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    bubble_manager_ = MakeBubbleManager();
  }
  void TearDownOnMainThread() override {
    bubble_manager_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  WebUIBubbleManager* bubble_manager() { return bubble_manager_.get(); }

  // WebContents under the ".top-chrome" pseudo-TLD will reuse the render
  // process.
  std::unique_ptr<WebUIBubbleManager> MakeBubbleManager(
      GURL site_url = GURL("chrome://test.top-chrome")) {
    return WebUIBubbleManager::Create<TestWebUIController>(
        BrowserView::GetBrowserViewForBrowser(browser()), browser()->profile(),
        site_url, 1);
  }

  void DestroyBubbleManager() { bubble_manager_.reset(); }

 private:
  std::unique_ptr<WebUIBubbleManager> bubble_manager_;
};

IN_PROC_BROWSER_TEST_F(WebUIBubbleManagerBrowserTest, CreateAndCloseBubble) {
  EXPECT_EQ(nullptr, bubble_manager()->GetBubbleWidget());
  bubble_manager()->ShowBubble();
  EXPECT_NE(nullptr, bubble_manager()->GetBubbleWidget());
  EXPECT_FALSE(bubble_manager()->GetBubbleWidget()->IsClosed());

  bubble_manager()->CloseBubble();
  EXPECT_TRUE(bubble_manager()->GetBubbleWidget()->IsClosed());
}

IN_PROC_BROWSER_TEST_F(WebUIBubbleManagerBrowserTest,
                       ShowUISetsBubbleWidgetVisible) {
  EXPECT_EQ(nullptr, bubble_manager()->GetBubbleWidget());
  bubble_manager()->ShowBubble();
  EXPECT_NE(nullptr, bubble_manager()->GetBubbleWidget());
  EXPECT_FALSE(bubble_manager()->GetBubbleWidget()->IsClosed());
  EXPECT_FALSE(bubble_manager()->GetBubbleWidget()->IsVisible());

  bubble_manager()->bubble_view_for_testing()->ShowUI();
  EXPECT_TRUE(bubble_manager()->GetBubbleWidget()->IsVisible());

  bubble_manager()->CloseBubble();
  EXPECT_TRUE(bubble_manager()->GetBubbleWidget()->IsClosed());
}

// Ensures that the WebUI bubble is destroyed synchronously with the manager.
// This guards against a potential UAF crash (see crbug.com/1345546).
IN_PROC_BROWSER_TEST_F(WebUIBubbleManagerBrowserTest,
                       ManagerDestructionClosesBubble) {
  EXPECT_EQ(nullptr, bubble_manager()->GetBubbleWidget());
  bubble_manager()->ShowBubble();
  EXPECT_NE(nullptr, bubble_manager()->GetBubbleWidget());

  base::WeakPtr<WebUIBubbleDialogView> bubble_view =
      bubble_manager()->bubble_view_for_testing();
  EXPECT_TRUE(bubble_view);
  bubble_view->ShowUI();
  EXPECT_TRUE(bubble_manager()->GetBubbleWidget()->IsVisible());

  // Destroy the bubble manager without explicitly destroying the bubble. Ensure
  // the bubble is closed synchronously.
  DestroyBubbleManager();
  EXPECT_FALSE(bubble_view);
}

// Verifies that the warm-up levels are correctly recorded.
// TODO(crbug.com/325316150): Fix flakiness and re-enable.
IN_PROC_BROWSER_TEST_F(WebUIBubbleManagerBrowserTest, DISABLED_WarmupLevel) {
  // Use the spare renderer if there is one.
  EXPECT_NE(content::RenderProcessHost::GetSpareRenderProcessHostForTesting(),
            nullptr);
  bubble_manager()->ShowBubble();
  EXPECT_EQ(bubble_manager()->contents_warmup_level(),
            WebUIContentsWarmupLevel::kSpareRenderer);

  // Create a new process if there is no spare renderer.
  DestroySpareRenderProcess();
  DestroyBubble(bubble_manager(), browser()->profile());
  bubble_manager()->ShowBubble();
  EXPECT_EQ(bubble_manager()->contents_warmup_level(),
            WebUIContentsWarmupLevel::kNoRenderer);

  // Use the process dedicated to top chrome WebUIs if there is one.
  DestroyBubble(bubble_manager(), browser()->profile());
  // Use a different domain under .top-chrome so that the WebContents
  // is not reused if WebUIBubblePerProfilePersistence is enabled.
  std::unique_ptr<WebUIBubbleManager> another_bubble_manager =
      MakeBubbleManager(GURL("chrome://test2.top-chrome"));
  another_bubble_manager->ShowBubble();
  bubble_manager()->ShowBubble();
  EXPECT_EQ(bubble_manager()->contents_warmup_level(),
            WebUIContentsWarmupLevel::kDedicatedRenderer);

  // Use the cached WebContents if there is one.
  bubble_manager()->CloseBubble();
  base::RunLoop().RunUntilIdle();
  bubble_manager()->ShowBubble();
  EXPECT_EQ(bubble_manager()->contents_warmup_level(),
            WebUIContentsWarmupLevel::kReshowingWebContents);
}
