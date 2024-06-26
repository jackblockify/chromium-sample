// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.account_storage_notice;

import android.content.Context;
import android.text.SpannableString;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.widget.ButtonCompat;

class AccountStorageNoticeView implements BottomSheetContent {
    private static boolean sSkipLayoutForTesting;

    private final View mContentView;

    // Initialized lazily by the corresponding setters.
    private Runnable mButtonCallback;
    private Runnable mSettingsLinkCallback;

    // TODO(crbug.com/341176706): Shadow the AccountStorageNoticeView constructor in the unit test
    // instead. There seems to be a problem with shadow and release builds.
    public static void setSkipLayoutForTesting(boolean skip) {
        sSkipLayoutForTesting = skip;
    }

    public AccountStorageNoticeView(Context context) {
        if (sSkipLayoutForTesting) {
            mContentView = null;
            return;
        }

        mContentView =
                LayoutInflater.from(context)
                        .inflate(R.layout.account_storage_notice_layout, /* root= */ null);
        ((ButtonCompat) mContentView.findViewById(R.id.account_storage_notice_button))
                .setOnClickListener(unused -> mButtonCallback.run());
        TextView linkView = mContentView.findViewById(R.id.account_storage_settings_link);
        SpannableString linkText =
                SpanApplier.applySpans(
                        context.getString(R.string.passwords_account_storage_notice_subtitle),
                        new SpanApplier.SpanInfo(
                                "<link>",
                                "</link>",
                                new NoUnderlineClickableSpan(
                                        context, unused -> mSettingsLinkCallback.run())));
        linkView.setText(linkText);
        linkView.setMovementMethod(LinkMovementMethod.getInstance());
    }

    public void setButtonCallback(Runnable buttonCallback) {
        mButtonCallback = buttonCallback;
    }

    public void setSettingsLinkCallback(Runnable settingsLinkCallback) {
        mSettingsLinkCallback = settingsLinkCallback;
    }

    @Override
    public View getContentView() {
        return mContentView;
    }

    @Override
    @Nullable
    public View getToolbarView() {
        return null;
    }

    @Override
    public int getVerticalScrollOffset() {
        return 0;
    }

    @Override
    public void destroy() {}

    @Override
    public @ContentPriority int getPriority() {
        return ContentPriority.HIGH;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return true;
    }

    @Override
    public int getPeekHeight() {
        return HeightMode.DISABLED;
    }

    @Override
    public float getFullHeightRatio() {
        return HeightMode.WRAP_CONTENT;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.passwords_account_storage_notice_title;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return R.string.passwords_account_storage_notice_half_height_accessibility_label;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.passwords_account_storage_notice_full_height_accessibility_label;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.passwords_account_storage_notice_closed_accessibility_label;
    }
}