// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALL_COMMAND_HELPER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALL_COMMAND_HELPER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "base/version.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
#include "url/gurl.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace webapps {
class WebAppUrlLoader;
enum class WebAppUrlLoaderResult;
}  // namespace webapps

namespace web_app {

enum class IconsDownloadedResult;
class IsolatedWebAppResponseReader;
class IsolatedWebAppStorageLocation;
class IsolatedWebAppResponseReaderFactory;
class IwaSourceWithMode;
class IwaSourceWithModeAndFileOp;
class UnusableSwbnFileError;
class WebAppDataRetriever;
// Copies the file being installed to the profile directory.
// On success returns a new owned location in the callback.
void UpdateBundlePathAndCreateStorageLocation(
    const base::FilePath& profile_dir,
    const IwaSourceWithModeAndFileOp& source,
    base::OnceCallback<void(
        base::expected<IsolatedWebAppStorageLocation, std::string>)> callback);

// Removes the IWA's randomly named directory in the profile directory.
// Calls the closure on complete.
void CleanupLocationIfOwned(const base::FilePath& profile_dir,
                            const IsolatedWebAppStorageLocation& location,
                            base::OnceClosure closure);

// This is a helper class that contains methods which are shared between both
// install and update commands.
class IsolatedWebAppInstallCommandHelper {
 public:
  static std::unique_ptr<IsolatedWebAppResponseReaderFactory>
  CreateDefaultResponseReaderFactory(Profile& profile);

  static std::unique_ptr<content::WebContents> CreateIsolatedWebAppWebContents(
      Profile& profile);

  IsolatedWebAppInstallCommandHelper(
      IsolatedWebAppUrlInfo url_info,
      std::unique_ptr<WebAppDataRetriever> data_retriever,
      std::unique_ptr<IsolatedWebAppResponseReaderFactory>
          response_reader_factory);
  ~IsolatedWebAppInstallCommandHelper();

  IsolatedWebAppInstallCommandHelper(
      const IsolatedWebAppInstallCommandHelper&) = delete;
  IsolatedWebAppInstallCommandHelper& operator=(
      const IsolatedWebAppInstallCommandHelper&) = delete;

  void CheckTrustAndSignatures(
      const IwaSourceWithMode& location,
      Profile* profile,
      base::OnceCallback<void(base::expected<void, std::string>)> callback);

  void CreateStoragePartitionIfNotPresent(Profile& profile);

  void LoadInstallUrl(
      const IwaSourceWithMode& source,
      content::WebContents& web_contents,
      webapps::WebAppUrlLoader& url_loader,
      base::OnceCallback<void(base::expected<void, std::string>)> callback);

  struct ManifestAndUrl {
    ManifestAndUrl(blink::mojom::ManifestPtr manifest, GURL url);
    ~ManifestAndUrl();

    ManifestAndUrl(const ManifestAndUrl&) = delete;
    ManifestAndUrl& operator=(const ManifestAndUrl&) = delete;

    ManifestAndUrl(ManifestAndUrl&&);
    ManifestAndUrl& operator=(ManifestAndUrl&&);

    blink::mojom::ManifestPtr manifest;
    GURL url;
  };

  void CheckInstallabilityAndRetrieveManifest(
      content::WebContents& web_contents,
      base::OnceCallback<void(base::expected<ManifestAndUrl, std::string>)>
          callback);

  base::expected<WebAppInstallInfo, std::string>
  ValidateManifestAndCreateInstallInfo(
      const std::optional<base::Version>& expected_version,
      const ManifestAndUrl& manifest_and_url);

  void RetrieveIconsAndPopulateInstallInfo(
      WebAppInstallInfo install_info,
      content::WebContents& web_contents,
      base::OnceCallback<void(base::expected<WebAppInstallInfo, std::string>)>
          callback);

 private:
  void CheckTrustAndSignaturesOfBundle(
      const base::FilePath& path,
      bool dev_mode,
      base::OnceCallback<void(base::expected<void, std::string>)> callback);

  void OnTrustAndSignaturesOfBundleChecked(
      base::OnceCallback<void(base::expected<void, std::string>)> callback,
      base::expected<std::unique_ptr<IsolatedWebAppResponseReader>,
                     UnusableSwbnFileError> status);

  void OnLoadInstallUrl(
      base::OnceCallback<void(base::expected<void, std::string>)> callback,
      webapps::WebAppUrlLoaderResult result);

  void OnCheckInstallabilityAndRetrieveManifest(
      base::OnceCallback<void(base::expected<ManifestAndUrl, std::string>)>
          callback,
      blink::mojom::ManifestPtr opt_manifest,
      const GURL& manifest_url,
      bool valid_manifest_for_web_app,
      webapps::InstallableStatusCode error_code);

  void OnRetrieveIcons(
      WebAppInstallInfo install_info,
      base::OnceCallback<void(base::expected<WebAppInstallInfo, std::string>)>
          callback,
      IconsDownloadedResult result,
      IconsMap icons_map,
      DownloadedIconsHttpResults unused_icons_http_results);

  IsolatedWebAppUrlInfo url_info_;
  std::unique_ptr<WebAppDataRetriever> data_retriever_;
  std::unique_ptr<IsolatedWebAppResponseReaderFactory> response_reader_factory_;

  base::WeakPtrFactory<IsolatedWebAppInstallCommandHelper> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALL_COMMAND_HELPER_H_