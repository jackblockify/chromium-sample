// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_UTIL_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_UTIL_H_

#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "ui/views/widget/widget.h"

class Profile;

namespace borealis {

// This is used by the Borealis installer app.
// Generated by crx_file::id_util::GenerateId("org.chromium.borealis");
extern const char kInstallerAppId[];
// This is the id of the main application which borealis runs.
extern const char kClientAppId[];
// This is an id for an app that looks like the steam client, which is shown to
// users in search who want to install it. Generated by:
// crx_file::id_util::GenerateId("org.chromium.borealis.installer.entrypoint").
// See b/243733660 for details.
extern const char kLauncherSearchAppId[];
// App IDs prefixed with this are unidentified and should be largely ignored.
// These are "anonymous" apps, created because a window couldn't be associated
// with any known app. When these appear in Borealis it's usually caused by
// the main app; but we can't rely on that, so just ignore them.
extern const char kIgnoredAppIdPrefix[];
// This is used to install the Borealis DLC component.
extern const char kBorealisDlcName[];
// Base64-encoded allowed x-scheme for Borealis apps.
extern const char kAllowedScheme[];
// Error string to replace Proton version info in the event that a GameID
// parsed with /usr/bin/get_compat_tool_versions.py in the Borealis VM does not
// match the GameID expected based on extraction with kBorealisAppIdRegex.
extern const char kCompatToolVersionGameMismatch[];
// Query parameter key for device information in the borealis feedback
// form.
extern const char kDeviceInformationKey[];

struct CompatToolInfo {
  std::optional<int> game_id;
  std::string proton = "None";
  std::string slr = "None";
};

// Returns true if it's a non game borealis app (e.g. Steam client).
// Note that this does not check if the app is from the Borealis VM.
bool IsNonGameBorealisApp(const std::string& app_id);

// Steam started putting "Proton X.0", "Steam Linux Runtime - XXX" apps in the
// launcher recently, we shouldn't show them.
//
// TODO(b/288176160): Valve probably shouldn't be doing this, we want a more
// thorough fix long term.
bool ShouldHideIrrelevantApp(
    const guest_os::GuestOsRegistryService::Registration& registration);

// Returns a Steam Game ID parsed from |exec|, or nullopt on failure.
// These are the numeric "App IDs" described at
// https://partner.steamgames.com/doc/store/application. We use the term
// "Steam Game ID" here to differentiate from other kinds of "application ID".
//
// TODO(b/173547790): This should probably be moved when we've decided
// the details of how/where it will be used.
std::optional<int> ParseSteamGameId(std::string exec);

// Returns the Steam Game ID of the |window|, or nullopt on failure.
// These are the numeric "App IDs" described at
// https://partner.steamgames.com/doc/store/application. We use the term
// "Steam Game ID" here to differentiate from other kinds of "application ID".
std::optional<int> SteamGameId(const aura::Window* window);

// Get the steam app id (a.k.a. STEAM_GAME cardinal) for the app with the
// given chromeos |app_id|, registered with |profile|, or nullopt if we can't
// work it out/there isn't one.
//
// Works for anonymous apps of the form "borealis_anon:.*xprop.<id>".
std::optional<int> SteamGameId(Profile* profile, const std::string& app_id);

// Checks that a given URL has the allowed scheme and that its contents starts
// with one of the URLs in the allowlist.
bool IsExternalURLAllowed(const GURL& url);

// Executes /usr/bin/get_compat_tool_versions.py in the borealis VM, which
// outputs the compat tool version information of any recent game session.
bool GetCompatToolInfo(const std::string& owner_id, std::string* output);

// Parses the output returned by GetCompatToolInfo.
CompatToolInfo ParseCompatToolInfo(std::optional<int> game_id,
                                   const std::string& output);

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_UTIL_H_
