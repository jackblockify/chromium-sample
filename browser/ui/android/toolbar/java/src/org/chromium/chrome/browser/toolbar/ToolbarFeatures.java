// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import androidx.annotation.OptIn;
import androidx.core.os.BuildCompat;

import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Utility class for toolbar code interacting with features and params. */
public final class ToolbarFeatures {

    private static Boolean sTabStripLayoutOptimizationEnabledForTesting;

    /** Private constructor to avoid instantiation. */
    private ToolbarFeatures() {}

    public static boolean shouldSuppressCaptures() {
        return ChromeFeatureList.sSuppressionToolbarCaptures.isEnabled();
    }

    /**
     * Returns whether to record metrics from suppression experiment. This allows an arm of
     * suppression to run without the overhead from reporting any extra metrics in Java. Using a
     * feature instead of a param to utilize Java side caching.
     */
    public static boolean shouldRecordSuppressionMetrics() {
        return ChromeFeatureList.sRecordSuppressionMetrics.isEnabled();
    }

    /** Returns if we are using optimized window layout for tab strip. */
    @OptIn(markerClass = androidx.core.os.BuildCompat.PrereleaseSdkCheck.class)
    public static boolean isTabStripWindowLayoutOptimizationEnabled(boolean isTablet) {
        if (sTabStripLayoutOptimizationEnabledForTesting != null) {
            return sTabStripLayoutOptimizationEnabledForTesting;
        }
        return ChromeFeatureList.sTabStripLayoutOptimization.isEnabled()
                && isTablet
                && BuildCompat.isAtLeastV();
    }

    /** Set the return value for {@link #isTabStripWindowLayoutOptimizationEnabled(boolean)}. */
    public static void setIsTabStripLayoutOptimizationEnabledForTesting(boolean enabled) {
        sTabStripLayoutOptimizationEnabledForTesting = enabled;
        ResettersForTesting.register(() -> sTabStripLayoutOptimizationEnabledForTesting = null);
    }
}
