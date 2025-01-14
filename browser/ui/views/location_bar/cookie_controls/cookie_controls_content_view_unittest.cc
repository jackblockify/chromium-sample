// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_content_view.h"

#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/grit/generated_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view.h"

using ::testing::Contains;
using ::testing::Eq;
using ::testing::Property;

class CookieControlsContentViewUnitTest : public TestWithBrowserView {
 public:
  CookieControlsContentViewUnitTest()
      : view_(std::make_unique<CookieControlsContentView>()) {}

 protected:
  views::View* GetFeedbackButton() {
    return views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
        CookieControlsContentView::kFeedbackButton,
        views::ElementTrackerViews::GetContextForView(
            view_->feedback_section_));
  }
  views::ToggleButton* GetToggleButton() { return view_->toggle_button_; }
  CookieControlsContentView* GetContentView() { return view_.get(); }

  std::unique_ptr<CookieControlsContentView> view_;
};

namespace {

TEST_F(CookieControlsContentViewUnitTest, FeedbackSection) {
  EXPECT_THAT(
      GetFeedbackButton()->GetViewAccessibility().GetCachedName(),
      Eq(base::JoinString(
          {l10n_util::GetStringUTF16(
               IDS_COOKIE_CONTROLS_BUBBLE_SEND_FEEDBACK_BUTTON_TITLE),
           l10n_util::GetStringUTF16(
               IDS_COOKIE_CONTROLS_BUBBLE_SEND_FEEDBACK_BUTTON_DESCRIPTION)},
          u" \n")));
}

TEST_F(CookieControlsContentViewUnitTest, ToggleButton_Initial) {
  EXPECT_THAT(GetToggleButton()->GetViewAccessibility().GetCachedName(),
              Eq(l10n_util::GetStringUTF16(
                  IDS_COOKIE_CONTROLS_BUBBLE_THIRD_PARTY_COOKIES_LABEL)));
}

TEST_F(CookieControlsContentViewUnitTest, ToggleButton_UpdatedSites) {
  const std::u16string label = u"17 sites allowed";
  GetContentView()->SetToggleLabel(label);
  std::u16string expected = base::JoinString(
      {l10n_util::GetStringUTF16(
           IDS_COOKIE_CONTROLS_BUBBLE_THIRD_PARTY_COOKIES_LABEL),
       label},
      u"\n");
  // TODO: convert to AllOf(HasSubstr(), HasSubStr()) when gtest supports
  // u16string.
  EXPECT_THAT(GetToggleButton()->GetViewAccessibility().GetCachedName(),
              Eq(expected));
}

}  // namespace
