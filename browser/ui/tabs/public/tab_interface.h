// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_PUBLIC_TAB_INTERFACE_H_
#define CHROME_BROWSER_UI_TABS_PUBLIC_TAB_INTERFACE_H_

#include <memory>

#include "base/callback_list.h"
#include "base/functional/callback.h"

namespace content {
class WebContents;
}  // namespace content

class BrowserWindowInterface;

namespace tabs {

class TabFeatures;

// A feature which wants to show tab-modal UI should call
// TabInterface::ShowModalUI and keep alive the instance of ScopedTabModalUI for
// the duration of the tab-modal UI.
class ScopedTabModalUI {
 public:
  ScopedTabModalUI() = default;
  virtual ~ScopedTabModalUI() = default;
};

// This is the public interface for tabs in a desktop browser. Most features in
// //chrome/browser depend on this interface, and thus to prevent circular
// dependencies this interface should not depend on anything else in //chrome.
// Ping erikchen for assistance if this class does not have the functionality
// your feature needs. This comment will be deleted after there are 10+ features
// in TabFeatures.
class TabInterface {
 public:
  // When a tab is in the background, the WebContents may be discarded to save
  // memory. When a tab is in the foreground it is guaranteed to have a
  // WebContents.
  virtual content::WebContents* GetContents() const = 0;

  // Register for this callback to detect changes to GetContents(). The first
  // WebContents is the contents that will be discarded. The second WebContents
  // is the new contents. The tab is guaranteed to be in the background.
  using WillDiscardContentsCallback = base::RepeatingCallback<
      void(TabInterface*, content::WebContents*, content::WebContents*)>;
  virtual base::CallbackListSubscription RegisterWillDiscardContents(
      WillDiscardContentsCallback callback) = 0;

  // Whether the tab is in the foreground. When a tab is in the foreground, this
  // class guarantees that GetContents() will return a non-nullptr WebContents,
  // and this WebContents will not change. If a tab is dragged out of a window
  // (creating a new window), it will briefly enter the background, and then
  // re-enter the foreground. To see if this is happened, check the
  // BrowserWindowInterface's session id.
  virtual bool IsInForeground() const = 0;

  // Register for these two callbacks to detect changes to IsInForeground().
  using DidEnterForegroundCallback =
      base::RepeatingCallback<void(TabInterface*)>;
  virtual base::CallbackListSubscription RegisterDidEnterForeground(
      DidEnterForegroundCallback callback) = 0;

  using WillEnterBackgroundCallback =
      base::RepeatingCallback<void(TabInterface*)>;
  virtual base::CallbackListSubscription RegisterWillEnterBackground(
      WillEnterBackgroundCallback callback) = 0;

  // Register for this callback to detect when a tab will be detached from a
  // window.
  enum class DetachReason {
    // The tab is about to be deleted.
    kDelete,
    // The tab is going to be removed, in order to be inserted into another
    // window.
    kInsertIntoOtherWindow
  };
  using WillDetach = base::RepeatingCallback<void(TabInterface*, DetachReason)>;
  virtual base::CallbackListSubscription RegisterWillDetach(
      WillDetach callback) = 0;

  // Features that want to show tab-modal UI are mutually exclusive. Before
  // showing a modal UI first check `CanShowModal`. Then call ShowModal() and
  // keep `ScopedTabModal` alive to prevent other features from showing
  // tab-modal UI.
  virtual bool CanShowModalUI() const = 0;
  virtual std::unique_ptr<ScopedTabModalUI> ShowModalUI() = 0;

  // A normal browser window has a tab strip and an omnibox. The returned value
  // never changes.
  virtual bool IsInNormalWindow() const = 0;

  // Check that IsInForeground() is `true` before calling this method. If a tab
  // is in the background there is no guarantee that it is associated with a
  // browser window.
  virtual BrowserWindowInterface* GetBrowserWindowInterface() = 0;

  // Returns the feature controllers scoped to this tab.
  virtual tabs::TabFeatures* GetTabFeatures() = 0;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_PUBLIC_TAB_INTERFACE_H_
