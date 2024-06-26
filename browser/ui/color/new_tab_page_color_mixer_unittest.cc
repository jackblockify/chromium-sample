// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/new_tab_page_color_mixer.h"

#include <optional>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/themes/custom_theme_supplier.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/common/themes/autogenerated_theme_util.h"
#include "components/search/ntp_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_recipe.h"
#include "ui/gfx/color_palette.h"

namespace {

using ThemeType = ui::ColorProviderKey::ThemeInitializerSupplier::ThemeType;

TEST(NewTabPageColorMixer, LightAndDarkThemeColors) {
  constexpr SkColor kToolbarColors[2] = {SK_ColorWHITE, gfx::kGoogleGrey900};
  for (const SkColor& toolbar_color : kToolbarColors) {
    ui::ColorProvider provider;
    ui::ColorMixer& mixer = provider.AddMixer();
    mixer[kColorToolbar] = {toolbar_color};
    ui::ColorProviderKey key;
    if (color_utils::IsDark(toolbar_color))
      key.color_mode = ui::ColorProviderKey::ColorMode::kDark;
    AddNewTabPageColorMixer(&provider, key);

    EXPECT_EQ(provider.GetColor(kColorToolbar), toolbar_color);
    EXPECT_EQ(provider.GetColor(kColorNewTabPageBackground), toolbar_color);
    EXPECT_EQ(provider.GetColor(kColorNewTabPageButtonBackground),
              toolbar_color);
    EXPECT_EQ(provider.GetColor(kColorNewTabPageModuleBackground),
              toolbar_color);
  }
}

TEST(NewTabPageColorMixer, CustomColorComprehensiveThemeColors) {
  ui::ColorProvider provider;
  ui::ColorMixer& mixer = provider.AddMixer();
  mixer[kColorToolbar] = {gfx::kGoogleGreen300};
  ui::ColorProviderKey key;
  key.custom_theme =
      base::WrapRefCounted(new CustomThemeSupplier(ThemeType::kAutogenerated));
  AddNewTabPageColorMixer(&provider, key);

  EXPECT_EQ(provider.GetColor(kColorToolbar), gfx::kGoogleGreen300);
  EXPECT_EQ(provider.GetColor(kColorNewTabPageBackground),
            gfx::kGoogleGreen300);
  SkColor contrasting_color = GetContrastingColor(gfx::kGoogleGreen300, 0.1f);
  EXPECT_EQ(provider.GetColor(kColorNewTabPageButtonBackground),
            contrasting_color);
  EXPECT_EQ(provider.GetColor(kColorNewTabPageMostVisitedTileBackground),
            contrasting_color);
}

TEST(NewTabPageColorMixer, DefaultColorComprehensiveThemeColor) {
  constexpr SkColor kSampleToolbarColor = SK_ColorWHITE;
  ui::ColorProvider provider;
  ui::ColorMixer& mixer = provider.AddMixer();
  mixer[kColorToolbar] = {kSampleToolbarColor};
  mixer[ui::kColorFrameActive] = {SK_ColorBLUE};
  ui::ColorProviderKey key;
  key.custom_theme =
      base::WrapRefCounted(new CustomThemeSupplier(ThemeType::kAutogenerated));
  AddNewTabPageColorMixer(&provider, key);

  EXPECT_EQ(provider.GetColor(kColorToolbar), kSampleToolbarColor);
  EXPECT_EQ(provider.GetColor(kColorNewTabPageBackground), kSampleToolbarColor);
  EXPECT_EQ(provider.GetColor(kColorNewTabPageButtonBackground),
            kSampleToolbarColor);
  EXPECT_EQ(provider.GetColor(kColorNewTabPageButtonForeground),
            gfx::kGoogleBlue600);
  EXPECT_EQ(provider.GetColor(kColorNewTabPageLink), gfx::kGoogleBlue600);
  EXPECT_EQ(provider.GetColor(kColorNewTabPageMostVisitedTileBackground),
            gfx::kGoogleGrey100);
}

}  // namespace
