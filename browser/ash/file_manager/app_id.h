// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_APP_ID_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_APP_ID_H_

namespace file_manager {

// The file manager's app ID.
//
// Note that file_manager::kFileManagerAppId is a bit redundant but a shorter
// name like kAppId would be cryptic inside "file_manager" namespace.
constexpr char kFileManagerAppId[] = "hhaomjibdihmijegdhdafkllkbggdgoj";

// The app ID generated by the System Web App framework.
constexpr char kFileManagerSwaAppId[] = "fkiggjmkendpmbegkagpmagjepfkpmeb";

// The text editor's app ID.
constexpr char kTextEditorAppId[] = "mmfbcljfglbokpmkimbfghdkjmjhdgbg";

// The image loader extension's ID.
constexpr char kImageLoaderExtensionId[] = "pmfjbimdmchhbnneeidfognadeopoehp";

}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_APP_ID_H_
