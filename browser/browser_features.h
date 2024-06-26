// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines the browser-specific base::FeatureList features that are
// not shared with other process types.

#ifndef CHROME_BROWSER_BROWSER_FEATURES_H_
#define CHROME_BROWSER_BROWSER_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.

BASE_DECLARE_FEATURE(kAutocompleteActionPredictorConfidenceCutoff);
BASE_DECLARE_FEATURE(kBookmarksTreeView);
BASE_DECLARE_FEATURE(kBookmarkTriggerForPrerender2);
BASE_DECLARE_FEATURE(kCertificateTransparencyAskBeforeEnabling);
BASE_DECLARE_FEATURE(kClosedTabCache);

BASE_DECLARE_FEATURE(kDestroyProfileOnBrowserClose);
BASE_DECLARE_FEATURE(kDestroySystemProfiles);

BASE_DECLARE_FEATURE(kDevToolsConsoleInsights);
extern const base::FeatureParam<std::string> kDevToolsConsoleInsightsAidaScope;
extern const base::FeatureParam<std::string>
    kDevToolsConsoleInsightsAidaEndpoint;
extern const base::FeatureParam<std::string> kDevToolsConsoleInsightsModelId;
extern const base::FeatureParam<double> kDevToolsConsoleInsightsTemperature;
extern const base::FeatureParam<bool> kDevToolsConsoleInsightsOptIn;

BASE_DECLARE_FEATURE(kDevToolsConsoleInsightsDogfood);
extern const base::FeatureParam<std::string>
    kDevToolsConsoleInsightsDogfoodAidaScope;
extern const base::FeatureParam<std::string>
    kDevToolsConsoleInsightsDogfoodAidaEndpoint;
extern const base::FeatureParam<std::string>
    kDevToolsConsoleInsightsDogfoodModelId;
extern const base::FeatureParam<double>
    kDevToolsConsoleInsightsDogfoodTemperature;
extern const base::FeatureParam<bool> kDevToolsConsoleInsightsDogfoodOptIn;

BASE_DECLARE_FEATURE(kDevToolsConsoleInsightsSettingVisible);
extern const base::FeatureParam<std::string>
    kDevToolsConsoleInsightsSettingVisibleBlockedReason;

BASE_DECLARE_FEATURE(kDevToolsFreestylerDogfood);

BASE_DECLARE_FEATURE(kDevToolsSharedProcessInfobar);
BASE_DECLARE_FEATURE(kDevToolsTabTarget);
BASE_DECLARE_FEATURE(kDevToolsVeLogging);

#if BUILDFLAG(IS_CHROMEOS)
BASE_DECLARE_FEATURE(kDoubleTapToZoomInTabletMode);
#endif

#if BUILDFLAG(IS_WIN)
BASE_DECLARE_FEATURE(kRegisterAppBoundEncryptionProvider);
BASE_DECLARE_FEATURE(kUseAppBoundEncryptionProviderForEncryption);
#endif

BASE_DECLARE_FEATURE(kFlexOrgManagementDisclosure);
BASE_DECLARE_FEATURE(kIncomingCallNotifications);
BASE_DECLARE_FEATURE(kKeyPinningComponentUpdater);

#if BUILDFLAG(IS_WIN)
BASE_DECLARE_FEATURE(kLockProfileCookieDatabase);
#endif

#if !BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kMuteNotificationSnoozeAction);
#endif

BASE_DECLARE_FEATURE(kNetworkAnnotationMonitoring);
BASE_DECLARE_FEATURE(kNewTabPageTriggerForPrerender2);
// This parameter is used to set a time threshold for triggering onMouseHover
// prerender. For example, if the value is 300, the New Tab Page prerender
// will start after 300ms after mouseHover duration is over 300ms.
const base::FeatureParam<int>
    kNewTabPagePrerenderStartDelayOnMouseHoverByMiliSeconds{
        &features::kNewTabPageTriggerForPrerender2,
        "prerender_start_delay_on_mouse_hover_ms", 300};
const base::FeatureParam<int>
    kNewTabPagePreconnectStartDelayOnMouseHoverByMiliSeconds{
        &features::kNewTabPageTriggerForPrerender2,
        "preconnect_start_delay_on_mouse_hover_ms", 100};
const base::FeatureParam<bool> kPrerenderNewTabPageOnMousePressedTrigger{
    &features::kNewTabPageTriggerForPrerender2,
    "prerender_new_tab_page_on_mouse_pressed_trigger", false};
const base::FeatureParam<bool> kPrerenderNewTabPageOnMouseHoverTrigger{
    &features::kNewTabPageTriggerForPrerender2,
    "prerender_new_tab_page_on_mouse_hover_trigger", false};

#if BUILDFLAG(IS_WIN)
BASE_DECLARE_FEATURE(kNoPreReadMainDll);
#endif

#if BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kNotificationOneTapUnsubscribe);
extern base::FeatureParam<bool>
    kNotificationOneTapUnsubscribeUseServiceIntentParam;
#endif

BASE_DECLARE_FEATURE(kOmniboxTriggerForPrerender2);

#if BUILDFLAG(IS_CHROMEOS)
BASE_DECLARE_FEATURE(kPlatformKeysAesEncryption);
#endif  // BUILDFLAG(IS_CHROMEOS)

BASE_DECLARE_FEATURE(kPrerenderDSEHoldback);
BASE_DECLARE_FEATURE(kPromoBrowserCommands);
extern const char kBrowserCommandIdParam[];

#if BUILDFLAG(IS_CHROMEOS_ASH)
BASE_DECLARE_FEATURE(kQuickSettingsPWANotifications);
#endif

#if !BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kReadAnythingPermanentAccessibility);
#endif

BASE_DECLARE_FEATURE(kRestartNetworkServiceUnsandboxedForFailedLaunch);
BASE_DECLARE_FEATURE(kSandboxExternalProtocolBlocked);
BASE_DECLARE_FEATURE(kSandboxExternalProtocolBlockedWarning);
BASE_DECLARE_FEATURE(kSupportSearchSuggestionForPrerender2);
// Indicates whether to make search prefetch response shareable to prerender.
// When allowing this, prerender can only copy the cache but cannot take over
// the ownership.
enum class SearchPreloadShareableCacheType {
  kEnabled,
  kDisabled,
};
extern const base::FeatureParam<SearchPreloadShareableCacheType>
    kSearchPreloadShareableCacheTypeParam;

BASE_DECLARE_FEATURE(kTriggerNetworkDataMigration);

#if BUILDFLAG(IS_CHROMEOS)
BASE_DECLARE_FEATURE(kTabCaptureBlueBorderCrOS);
#endif

BASE_DECLARE_FEATURE(kUseOsCryptAsyncForCookieEncryption);
BASE_DECLARE_FEATURE(kWebUsbDeviceDetection);

#if BUILDFLAG(IS_WIN)
BASE_DECLARE_FEATURE(kBrowserDynamicCodeDisabled);
#endif
}  // namespace features

#endif  // CHROME_BROWSER_BROWSER_FEATURES_H_