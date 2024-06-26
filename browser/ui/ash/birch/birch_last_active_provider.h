// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_LAST_ACTIVE_PROVIDER_H_
#define CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_LAST_ACTIVE_PROVIDER_H_

#include <string>

#include "ash/birch/birch_data_provider.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon_base/favicon_types.h"
#include "components/history/core/browser/history_types.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

class Profile;

namespace favicon {
class FaviconService;
}

namespace history {
class HistoryService;
}

namespace ash {

// Queries the last active URL for the birch feature. URLs are sent to
// 'BirchModel' to be stored.
class BirchLastActiveProvider : public BirchDataProvider {
 public:
  explicit BirchLastActiveProvider(Profile* profile);
  BirchLastActiveProvider(const BirchLastActiveProvider&) = delete;
  BirchLastActiveProvider& operator=(const BirchLastActiveProvider&) = delete;
  ~BirchLastActiveProvider() override;

  // BirchDataProvider:
  void RequestBirchDataFetch() override;

  // Callback from history service with the last active URL.
  void OnGotHistory(history::QueryResults results);

  // Callback from favicon service with the icon image.
  void OnGotFaviconImage(const std::u16string& title,
                         const GURL& url,
                         base::Time last_visit,
                         const favicon_base::FaviconImageResult& image_result);

  void set_history_service_for_test(history::HistoryService* service) {
    history_service_ = service;
  }
  void set_favicon_service_for_test(favicon::FaviconService* service) {
    favicon_service_ = service;
  }

 private:
  const raw_ptr<Profile> profile_;
  raw_ptr<history::HistoryService> history_service_;
  raw_ptr<favicon::FaviconService> favicon_service_;

  // Data from the previous fetch. Used to avoid re-fetching the icon.
  GURL previous_url_;
  gfx::Image previous_image_;

  // Task tracker for history requests.
  base::CancelableTaskTracker cancelable_task_tracker_;

  base::WeakPtrFactory<BirchLastActiveProvider> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_LAST_ACTIVE_PROVIDER_H_
