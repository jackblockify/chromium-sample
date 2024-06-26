// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_PREINSTALLED_WEB_APPS_GOOGLE_DOCS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_PREINSTALLED_WEB_APPS_GOOGLE_DOCS_H_

#include "chrome/browser/web_applications/external_install_options.h"

namespace web_app {

ExternalInstallOptions GetConfigForGoogleDocs(bool is_standalone_tabbed);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_PREINSTALLED_WEB_APPS_GOOGLE_DOCS_H_
