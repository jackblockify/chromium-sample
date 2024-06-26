// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Color;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;
import androidx.annotation.StyleRes;
import androidx.core.content.ContextCompat;

import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.util.ColorUtils;
import org.chromium.ui.util.ValueUtils;

/** Util class to handle various color operations shared between hub classes. */
public final class HubColors {
    private static final int[][] SELECTED_AND_NORMAL_STATES =
            new int[][] {new int[] {android.R.attr.state_selected}, new int[] {}};
    private static final int[][] DISABLED_AND_NORMAL_STATES =
            new int[][] {new int[] {-android.R.attr.state_enabled}, new int[] {}};

    private HubColors() {}

    /** Returns the color scheme from a pane with a fallback for null. */
    public static @HubColorScheme int getColorSchemeSafe(@Nullable Pane pane) {
        return pane == null ? HubColorScheme.DEFAULT : pane.getColorScheme();
    }

    /** Returns the background color generic surfaces should use per the given color scheme. */
    public static @ColorInt int getBackgroundColor(
            Context context, @HubColorScheme int colorScheme) {
        switch (colorScheme) {
            case HubColorScheme.DEFAULT:
                return SemanticColorUtils.getDefaultBgColor(context);
            case HubColorScheme.INCOGNITO:
                return ContextCompat.getColor(context, R.color.default_bg_color_dark);
            default:
                assert false;
                return Color.TRANSPARENT;
        }
    }

    /** Returns the color most icons should use per the given color scheme. */
    public static ColorStateList getIconColor(Context context, @HubColorScheme int colorScheme) {
        switch (colorScheme) {
            case HubColorScheme.DEFAULT:
                return ContextCompat.getColorStateList(
                        context, R.color.default_icon_color_tint_list);
            case HubColorScheme.INCOGNITO:
                return ContextCompat.getColorStateList(
                        context, R.color.default_icon_color_light_tint_list);
            default:
                assert false;
                return ColorStateList.valueOf(Color.TRANSPARENT);
        }
    }

    /** Returns the color selected icons should use per the given color scheme. */
    public static @ColorInt int getSelectedIconColor(
            Context context, @HubColorScheme int colorScheme) {
        switch (colorScheme) {
            case HubColorScheme.DEFAULT:
                return SemanticColorUtils.getDefaultIconColorAccent1(context);
            case HubColorScheme.INCOGNITO:
                return ContextCompat.getColor(context, R.color.default_control_color_active_dark);
            default:
                assert false;
                return Color.TRANSPARENT;
        }
    }

    /** Returns the color of secondary contains that reacts to being disabled. */
    public static ColorStateList getSecondaryContainerColorStateList(
            Context context, @HubColorScheme int colorScheme) {
        Resources resources = context.getResources();
        @ColorInt int color = getSecondaryContainerColor(context, colorScheme);
        float alpha = ValueUtils.getFloat(resources, R.dimen.filled_button_bg_disabled_alpha);
        int[] colors = new int[] {ColorUtils.setAlphaComponentWithFloat(color, alpha), color};
        return new ColorStateList(DISABLED_AND_NORMAL_STATES, colors);
    }

    /** Returns the color of secondary contains like the floating action button. */
    public static @ColorInt int getSecondaryContainerColor(
            Context context, @HubColorScheme int colorScheme) {
        switch (colorScheme) {
            case HubColorScheme.DEFAULT:
                return SemanticColorUtils.getChipBgSelectedColor(context);
            case HubColorScheme.INCOGNITO:
                return ContextCompat.getColor(context, R.color.baseline_secondary_30);
            default:
                assert false;
                return Color.TRANSPARENT;
        }
    }

    /** Returns the color most text should use for the given color scheme. */
    public static @StyleRes int getTextAppearanceMedium(@HubColorScheme int colorScheme) {
        switch (colorScheme) {
            case HubColorScheme.DEFAULT:
                return R.style.TextAppearance_TextMedium_Primary;
            case HubColorScheme.INCOGNITO:
                return R.style.TextAppearance_TextMedium_Primary_Baseline_Light;
            default:
                assert false;
                return Resources.ID_NULL;
        }
    }

    /** Convenience method to make a selectable {@link ColorStateList} from two input colors. */
    public static ColorStateList getSelectableIconList(
            @ColorInt int selectedColor, @ColorInt int normalColor) {
        int[] colors = new int[] {selectedColor, normalColor};
        return new ColorStateList(SELECTED_AND_NORMAL_STATES, colors);
    }
}