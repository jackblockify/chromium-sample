// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/performance_controls/performance_controls_metrics.h"
#include "chrome/browser/ui/performance_controls/performance_intervention_button_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/performance_controls/performance_intervention_bubble.h"
#include "chrome/browser/ui/views/performance_controls/performance_intervention_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/resource_attribution/page_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/views/view.h"

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTab);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kThirdTab);
constexpr char kSkipPixelTestsReason[] = "Should only run in pixel_tests.";

}  // namespace

class PerformanceInterventionInteractiveTest
    : public InteractiveFeaturePromoTest {
 public:
  PerformanceInterventionInteractiveTest()
      : InteractiveFeaturePromoTest(UseDefaultTrackerAllowingPromos(
            {feature_engagement::kIPHPerformanceInterventionDialogFeature})) {}
  ~PerformanceInterventionInteractiveTest() override = default;

  void SetUp() override {
    set_open_about_blank_on_browser_launch(true);
    feature_list_.InitWithFeatures(
        {performance_manager::features::kPerformanceIntervention,
         performance_manager::features::kPerformanceInterventionUI},
        {});
    InteractiveFeaturePromoTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveFeaturePromoTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  GURL GetURL(std::string_view hostname = "example.com",
              std::string_view path = "/title1.html") {
    return embedded_test_server()->GetURL(hostname, path);
  }

  std::vector<resource_attribution::PageContext> GetPageContextForTabs(
      const std::vector<int>& tab_indices) {
    std::vector<resource_attribution::PageContext> page_contexts;
    TabStripModel* const tab_strip_model = browser()->tab_strip_model();
    for (int index : tab_indices) {
      content::WebContents* const web_contents =
          tab_strip_model->GetWebContentsAt(index);
      std::optional<resource_attribution::PageContext> context =
          resource_attribution::PageContext::FromWebContents(web_contents);
      CHECK(context.has_value());
      page_contexts.push_back(context.value());
    }

    return page_contexts;
  }

  auto TriggerOnActionableTabListChange(const std::vector<int>& tab_indices) {
    return Do([&]() {
      BrowserView* const browser_view =
          BrowserView::GetBrowserViewForBrowser(browser());
      PerformanceInterventionButtonController* const controller =
          browser_view->toolbar()
              ->performance_intervention_button()
              ->controller();
      controller->OnActionableTabListChanged(
          PerformanceDetectionManager::ResourceType::kCpu,
          GetPageContextForTabs(tab_indices));
    });
  }

  auto CloseTab(int index) {
    return Do(base::BindLambdaForTesting([=]() {
      browser()->tab_strip_model()->CloseWebContentsAt(
          index, TabCloseTypes::CLOSE_NONE);
    }));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PerformanceInterventionInteractiveTest,
                       ShowAndHideButton) {
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GetURL()),
      TriggerOnActionableTabListChange({0}),
      WaitForShow(kToolbarPerformanceInterventionButtonElementId),
      WaitForShow(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody),
      // Flush the event queue to ensure that we trigger the button
      // to hide after it is shown.
      FlushEvents(),
      PressButton(kToolbarPerformanceInterventionButtonElementId),
      WaitForHide(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody),
      EnsurePresent(kToolbarPerformanceInterventionButtonElementId),
      FlushEvents(), TriggerOnActionableTabListChange({}),
      WaitForHide(kToolbarPerformanceInterventionButtonElementId));
}

IN_PROC_BROWSER_TEST_F(PerformanceInterventionInteractiveTest,
                       LimitShowingButton) {
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GetURL()),
      EnsureNotPresent(kToolbarPerformanceInterventionButtonElementId),
      TriggerOnActionableTabListChange({0}),
      WaitForShow(kToolbarPerformanceInterventionButtonElementId),
      WaitForShow(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody),
      PressButton(kToolbarPerformanceInterventionButtonElementId),
      WaitForHide(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody),
      // Flush the event queue to ensure that we trigger the button to hide
      // after it is shown.
      FlushEvents(), TriggerOnActionableTabListChange({}),
      WaitForHide(kToolbarPerformanceInterventionButtonElementId),
      FlushEvents(), TriggerOnActionableTabListChange({0}),
      EnsureNotPresent(kToolbarPerformanceInterventionButtonElementId));
}

// Making an actionable tab active should hide the intervention toolbar button
// because the actionable tab list is no longer valid.
IN_PROC_BROWSER_TEST_F(PerformanceInterventionInteractiveTest,
                       DISABLED_ActivateActionableTab) {
  RunTestSequence(
      InstrumentTab(kFirstTab, 0), AddInstrumentedTab(kSecondTab, GetURL()),
      AddInstrumentedTab(kThirdTab, GetURL()),
      EnsureNotPresent(kToolbarPerformanceInterventionButtonElementId),
      TriggerOnActionableTabListChange({0, 1}),
      WaitForShow(kToolbarPerformanceInterventionButtonElementId),
      WaitForShow(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody),
      PressButton(kToolbarPerformanceInterventionButtonElementId),
      WaitForHide(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody),
      EnsurePresent(kToolbarPerformanceInterventionButtonElementId),
      // Flush the event queue to ensure that we trigger the button to hide
      // after it is shown.
      FlushEvents(), SelectTab(kTabStripElementId, 0), WaitForShow(kFirstTab),
      WaitForHide(kToolbarPerformanceInterventionButtonElementId));
}

// The intervention toolbar button should remain visible after closing an
// actionable tab is there are more tabs that are still actionable.
IN_PROC_BROWSER_TEST_F(PerformanceInterventionInteractiveTest,
                       CloseActionableTab) {
  RunTestSequence(
      InstrumentTab(kFirstTab, 0), AddInstrumentedTab(kSecondTab, GetURL()),
      AddInstrumentedTab(kThirdTab, GetURL()),
      EnsureNotPresent(kToolbarPerformanceInterventionButtonElementId),
      TriggerOnActionableTabListChange({0, 1}),
      WaitForShow(kToolbarPerformanceInterventionButtonElementId),
      // Flush the event queue to ensure that we trigger the button to hide
      // after it is shown.
      FlushEvents(), CloseTab(1),
      // Button should still be showing since there is another actionable tab
      EnsurePresent(kToolbarPerformanceInterventionButtonElementId),
      CloseTab(0), WaitForHide(kToolbarPerformanceInterventionButtonElementId));
}

// Pixel test to verify that the performance intervention toolbar
// button looks correct.
IN_PROC_BROWSER_TEST_F(PerformanceInterventionInteractiveTest,
                       InterventionToolbarButton) {
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GetURL()),
      TriggerOnActionableTabListChange({0}),
      WaitForShow(kToolbarPerformanceInterventionButtonElementId),
      // Flush the event queue to ensure that the screenshot happens
      // after the button is shown.
      FlushEvents(),
      PressButton(kToolbarPerformanceInterventionButtonElementId),
      WaitForHide(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody),
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kSkipPixelTestsReason),
      Screenshot(kToolbarPerformanceInterventionButtonElementId,
                 /*screenshot_name=*/"InterventionToolbarButton",
                 /*baseline_cl=*/"5503223"));
}

// Dialog toggles between open and close when clicking on toolbar button
IN_PROC_BROWSER_TEST_F(PerformanceInterventionInteractiveTest,
                       DialogRespondsToToolbarButtonClick) {
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GetURL()),
      TriggerOnActionableTabListChange({0}),
      WaitForShow(kToolbarPerformanceInterventionButtonElementId),
      WaitForShow(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody),
      PressButton(kToolbarPerformanceInterventionButtonElementId),
      WaitForHide(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody),
      FlushEvents(),
      PressButton(kToolbarPerformanceInterventionButtonElementId),
      WaitForShow(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody));
}

// While the dialog is already visible, any changes to the actionable tab list
// should not affect the button and dialog visibility.
IN_PROC_BROWSER_TEST_F(PerformanceInterventionInteractiveTest,
                       DialogUnaffectedByActionableTabChange) {
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GetURL()),
      AddInstrumentedTab(kThirdTab, GetURL()),
      TriggerOnActionableTabListChange({0}),
      WaitForShow(kToolbarPerformanceInterventionButtonElementId),
      WaitForShow(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody),
      FlushEvents(),
      // Triggering the actionable tab list again shouldn't affect
      // dialog visibility
      TriggerOnActionableTabListChange({0, 1}),
      EnsurePresent(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody),
      FlushEvents(),
      // Dialog should stay open even though no tabs are actionable
      TriggerOnActionableTabListChange({}),
      EnsurePresent(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody));
}

// If the actionable tab list becomes empty while the intervention dialog is
// showing, after the dialog closes, the button should hide since there are no
// actionable tabs.
IN_PROC_BROWSER_TEST_F(PerformanceInterventionInteractiveTest,
                       ButtonHidesAfterDialogCloses) {
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GetURL()),
      AddInstrumentedTab(kThirdTab, GetURL()),
      TriggerOnActionableTabListChange({0}),
      WaitForShow(kToolbarPerformanceInterventionButtonElementId),
      WaitForShow(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody),
      FlushEvents(),
      // Triggering the actionable tab list again shouldn't affect
      // dialog visibility
      TriggerOnActionableTabListChange({}),
      EnsurePresent(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody),
      FlushEvents(),
      PressButton(PerformanceInterventionBubble::
                      kPerformanceInterventionDialogDismissButton),
      WaitForHide(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody),
      WaitForHide(kToolbarPerformanceInterventionButtonElementId));
}

// Clicking the dismiss dialog button should keep the toolbar button if the
// actionable tab list didn't become empty while the dialog was open.
IN_PROC_BROWSER_TEST_F(PerformanceInterventionInteractiveTest,
                       ButtonStaysAfterDismissClicked) {
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GetURL()),
      AddInstrumentedTab(kThirdTab, GetURL()),
      TriggerOnActionableTabListChange({0}),
      WaitForShow(kToolbarPerformanceInterventionButtonElementId),
      WaitForShow(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody),
      FlushEvents(),
      PressButton(PerformanceInterventionBubble::
                      kPerformanceInterventionDialogDismissButton),
      WaitForHide(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody),
      EnsurePresent(kToolbarPerformanceInterventionButtonElementId));
}

// Clicking the deactivate dialog button should immediately hide the performance
// intervention toolbar button because the user enacted the suggested action.
IN_PROC_BROWSER_TEST_F(PerformanceInterventionInteractiveTest,
                       ButtonHidesAfterDeactivateClicked) {
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GetURL()),
      AddInstrumentedTab(kThirdTab, GetURL()),
      TriggerOnActionableTabListChange({0}),
      WaitForShow(kToolbarPerformanceInterventionButtonElementId),
      WaitForShow(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody),
      FlushEvents(),
      PressButton(PerformanceInterventionBubble::
                      kPerformanceInterventionDialogDeactivateButton),
      WaitForHide(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody),
      WaitForHide(kToolbarPerformanceInterventionButtonElementId));
}

class PerformanceInterventionNonUiMetricsTest
    : public PerformanceInterventionInteractiveTest {
 public:
  void SetUp() override {
    set_open_about_blank_on_browser_launch(true);
    feature_list_.InitWithFeatures(
        {performance_manager::features::kPerformanceIntervention}, {});
    InteractiveFeaturePromoTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PerformanceInterventionNonUiMetricsTest,
                       TriggerMetricsRecorded) {
  base::HistogramTester histogram_tester;
  const std::string message_trigger_reason_histogram_name =
      "PerformanceControls.Intervention.BackgroundTab.Cpu.MessageTriggerResult";
  RunTestSequence(AddInstrumentedTab(kSecondTab, GetURL()),
                  AddInstrumentedTab(kThirdTab, GetURL()),
                  SelectTab(kTabStripElementId, 0), Do([&]() {
                    // verify that metrics were recorded
                    histogram_tester.ExpectBucketCount(
                        message_trigger_reason_histogram_name,
                        InterventionMessageTriggerResult::kShown, 0);
                    histogram_tester.ExpectBucketCount(
                        message_trigger_reason_histogram_name,
                        InterventionMessageTriggerResult::kRateLimited, 0);
                  }),
                  TriggerOnActionableTabListChange({1, 2}), Do([&]() {
                    // verify that metrics were recorded
                    histogram_tester.ExpectBucketCount(
                        message_trigger_reason_histogram_name,
                        InterventionMessageTriggerResult::kShown, 1);
                    histogram_tester.ExpectBucketCount(
                        message_trigger_reason_histogram_name,
                        InterventionMessageTriggerResult::kRateLimited, 0);
                  }),
                  TriggerOnActionableTabListChange({1, 2}), Do([&]() {
                    // verify that metrics were recorded
                    histogram_tester.ExpectBucketCount(
                        message_trigger_reason_histogram_name,
                        InterventionMessageTriggerResult::kShown, 1);
                    histogram_tester.ExpectBucketCount(
                        message_trigger_reason_histogram_name,
                        InterventionMessageTriggerResult::kRateLimited, 1);
                  }));
}
