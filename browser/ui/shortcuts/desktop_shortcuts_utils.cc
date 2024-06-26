// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/shortcuts/desktop_shortcuts_utils.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_type.h"
#include "content/public/common/url_constants.h"

namespace shortcuts {

bool CanCreateDesktopShortcut(Browser* browser) {
  Profile* profile = browser->profile();

  // Do not allow for Guest or OTR profiles.
  // System profiles have not been introduced here because they do not have a
  // browser.
  if (!profile || profile->IsGuestSession() || profile->IsOffTheRecord()) {
    return false;
  }

  // Do not allow if the web_contents appear to be crashing.
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents || web_contents->IsCrashed()) {
    return false;
  }

  // Do not allow if the site_url is invalid or doesn't have a HTTP/HTTPS
  // scheme.
  const GURL site_url = web_contents->GetLastCommittedURL();
  if (!site_url.is_valid() || !site_url.SchemeIsHTTPOrHTTPS()) {
    return false;
  }

  // Do not allow for error pages (like network errors etc).
  content::NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();
  if (entry && entry->GetPageType() == content::PAGE_TYPE_ERROR) {
    return false;
  }

  return true;
}

}  // namespace shortcuts
