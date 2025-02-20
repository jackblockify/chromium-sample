// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_OVERLAY_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_UI_LENS_LENS_OVERLAY_SIDE_PANEL_COORDINATOR_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_observer.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents_observer.h"

class Browser;
class GURL;
class LensOverlayController;
class LensOverlaySidePanelWebView;
class SidePanelUI;

namespace content {
class WebContents;
}  // namespace content

namespace views {
class View;
}  // namespace views

namespace lens {

// Handles the creation and registration of the lens overlay side panel entry.
// There are two ways for this instance to be torn down.
//   (1) Its owner, LensOverlayController can destroy it.
//   (2) `side_panel_web_view_` can be destroyed by the side panel.
// In the case of (2), this instance is no longer functional and needs to be
// torn down. There are two constraints:
//   (2a) The shutdown path of LensOverlayController must be asynchronous. This
//   avoids re-entrancy into the code that is in turn calling (2).
//   (2b) Clearing local state associated with `side_panel_web_view_` must be
//   done synchronously.
class LensOverlaySidePanelCoordinator : public SidePanelEntryObserver,
                                        public content::WebContentsObserver {
 public:
  LensOverlaySidePanelCoordinator(
      Browser* browser,
      LensOverlayController* lens_overlay_controller,
      SidePanelUI* side_panel_ui,
      content::WebContents* web_contents);
  LensOverlaySidePanelCoordinator(const LensOverlaySidePanelCoordinator&) =
      delete;
  LensOverlaySidePanelCoordinator& operator=(
      const LensOverlaySidePanelCoordinator&) = delete;
  ~LensOverlaySidePanelCoordinator() override;

  // Handles activations of the Lens overlay side panel entry.
  static actions::ActionItem::InvokeActionCallback
  CreateSidePanelActionCallback(Browser* browser);

  // Registers the side panel entry in the side panel if it doesn't already
  // exist and then shows it.
  void RegisterEntryAndShow();

  // SidePanelEntryObserver:
  void OnEntryHidden(SidePanelEntry* entry) override;

  // Called by the destructor of the side panel web view.
  void WebViewClosing();

  content::WebContents* GetSidePanelWebContents();

  // Whether the lens overlay entry is currently the active entry in the side
  // panel UI.
  bool IsEntryShowing();

 private:
  // content::WebContentsObserver:
  void DidOpenRequestedURL(content::WebContents* new_contents,
                           content::RenderFrameHost* source_render_frame_host,
                           const GURL& url,
                           const content::Referrer& referrer,
                           WindowOpenDisposition disposition,
                           ui::PageTransition transition,
                           bool started_from_context_menu,
                           bool renderer_initiated) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DOMContentLoaded(content::RenderFrameHost* render_frame_host) override;

  // Opens the provided url params in the main browser as a new tab.
  void OpenURLInBrowser(const content::OpenURLParams& params);

  // Registers the entry in the side panel if it doesn't already exist.
  void RegisterEntry();

  // Called to get the URL for the "open in new tab" button.
  GURL GetOpenInNewTabUrl();

  // Gets the tab web contents where this side panel was opened.
  content::WebContents* GetTabWebContents();

  std::unique_ptr<views::View> CreateLensOverlayResultsView();

  // The browser of the tab web contents passed by the overlay.
  const raw_ptr<Browser> tab_browser_;

  // Owns this.
  const raw_ptr<LensOverlayController> lens_overlay_controller_;

  // The side panel UI corresponding to the tab's browser.
  const raw_ptr<SidePanelUI> side_panel_ui_;

  base::WeakPtr<content::WebContents> tab_web_contents_;
  raw_ptr<LensOverlaySidePanelWebView> side_panel_web_view_;
  base::WeakPtrFactory<LensOverlaySidePanelCoordinator> weak_ptr_factory_{this};
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_OVERLAY_SIDE_PANEL_COORDINATOR_H_
