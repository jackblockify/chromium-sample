// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_MEMORY_SAVER_BUBBLE_DELEGATE_H_
#define CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_MEMORY_SAVER_BUBBLE_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/performance_controls/memory_saver_bubble_observer.h"
#include "chrome/browser/ui/performance_controls/performance_controls_metrics.h"
#include "ui/base/models/dialog_model.h"

// This class is the delegate for the memory saver bubble dialog that handles
// the events raised from the dialog.
class MemorySaverBubbleDelegate : public ui::DialogModelDelegate {
 public:
  explicit MemorySaverBubbleDelegate(Browser* browser,
                                     MemorySaverBubbleObserver* observer);

  void OnSettingsClicked();
  void OnAddSiteToTabDiscardExceptionsListClicked();

  void OnDialogDestroy();

 private:
  raw_ptr<Browser> browser_;
  raw_ptr<MemorySaverBubbleObserver> observer_;
  MemorySaverBubbleActionType close_action_ =
      MemorySaverBubbleActionType::kDismiss;
};

#endif  // CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_MEMORY_SAVER_BUBBLE_DELEGATE_H_
