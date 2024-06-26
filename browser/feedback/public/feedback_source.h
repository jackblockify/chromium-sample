// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEEDBACK_PUBLIC_FEEDBACK_SOURCE_H_
#define CHROME_BROWSER_FEEDBACK_PUBLIC_FEEDBACK_SOURCE_H_

// Sources of feedback requests.
//
// WARNING: The below enum MUST never be renamed, modified or reordered, as
// they're written to logs. You can only insert a new element immediately
// before the last. Also, 'FeedbackSource' in
// 'tools/metrics/histograms/enums.xml' MUST be kept in sync with the enum
// below.
// Note: Many feedback sources are being deprecated, or don't apply for Lacros
// (e.g. Ash only). Therefore, we won't support all the values listed below in
// Lacros. "enum LacrosFeedbackSource" in chromeos/crosapi/mojom/feedback.mojom
// lists all the feedback sources we allow in Lacros to the current. When you
// need to show feedack from Lacros with a new feedback source, please add it to
// LacrosFeedbackSource, handles the mojom serialization accordingly, and add a
// new test case in:
// chrome/browser/feedback/show_feedback_page_lacros_browsertest.cc.
namespace feedback {

enum FeedbackSource {
  kFeedbackSourceArcApp = 0,
  kFeedbackSourceAsh,
  kFeedbackSourceBrowserCommand,
  kFeedbackSourceMdSettingsAboutPage,
  kFeedbackSourceOldSettingsAboutPage,
  kFeedbackSourceProfileErrorDialog,
  kFeedbackSourceSadTabPage,
  kFeedbackSourceSupervisedUserInterstitial,
  kFeedbackSourceAssistant,
  kFeedbackSourceDesktopTabGroups,
  kFeedbackSourceMediaApp,
  kFeedbackSourceHelpApp,
  kFeedbackSourceKaleidoscope,
  kFeedbackSourceNetworkHealthPage,
  kFeedbackSourceTabSearch,
  kFeedbackSourceCameraApp,
  kFeedbackSourceCaptureMode,
  kFeedbackSourceChromeLabs,
  kFeedbackSourceBentoBar_DEPRECATED,
  kFeedbackSourceQuickAnswers,
  kFeedbackSourceWhatsNew,
  kFeedbackSourceConnectivityDiagnostics,
  kFeedbackSourceProjectorApp,
  kFeedbackSourceDesksTemplates,
  kFeedbackSourceFilesApp,
  kFeedbackSourceChannelIndicator,
  kFeedbackSourceLauncher,
  kFeedbackSourceSettingsPerformancePage,
  kFeedbackSourceQuickOffice,
  kFeedbackSourceOsSettingsSearch,
  kFeedbackSourceAutofillContextMenu,
  kFeedbackSourceUnknownLacrosSource,
  kFeedbackSourceWindowLayoutMenu,
  kFeedbackSourcePriceInsights,
  kFeedbackSourceCookieControls,
  kFeedbackSourceGameDashboard,
  kFeedbackSourceLensOverlay,
  kFeedbackSourceLogin,
  kFeedbackSourceAI,
  kFeedbackSourceFocusMode,
  kFeedbackSourceOverview,
  kFeedbackSourceSnapGroups_DEPRECATED,
  kFeedbackSourceBirch,
  kFeedbackSourceBorealis,

  // ATTENTION: Before making any changes or adding to feedback collection,
  // please ensure the teams that operationalize feedback are aware and
  // supportive. Contact: chrome-gtech@

  // Must be last.
  kFeedbackSourceCount,
};

}  // namespace feedback

#endif  // CHROME_BROWSER_FEEDBACK_PUBLIC_FEEDBACK_SOURCE_H_
