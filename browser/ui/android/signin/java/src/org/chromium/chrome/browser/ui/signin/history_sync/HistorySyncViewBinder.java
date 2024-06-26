// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.history_sync;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

class HistorySyncViewBinder {
    public static void bind(PropertyModel model, HistorySyncView view, PropertyKey key) {
        if (key == HistorySyncProperties.PROFILE_DATA) {
            view.getAccountImageView()
                    .setImageDrawable(model.get(HistorySyncProperties.PROFILE_DATA).getImage());
        } else if (key == HistorySyncProperties.ON_ACCEPT_CLICKED) {
            if (view.getAcceptButton() != null) {
                view.getAcceptButton()
                        .setOnClickListener(model.get(HistorySyncProperties.ON_ACCEPT_CLICKED));
            }
        } else if (key == HistorySyncProperties.ON_DECLINE_CLICKED) {
            if (view.getDeclineButton() != null) {
                view.getDeclineButton()
                        .setOnClickListener(model.get(HistorySyncProperties.ON_DECLINE_CLICKED));
            }
        } else if (key == HistorySyncProperties.FOOTER_STRING) {
            view.getDetailsDescription().setText(model.get(HistorySyncProperties.FOOTER_STRING));
        } else if (key == HistorySyncProperties.MINOR_MODE_RESTRICTION_STATUS
                || key == HistorySyncProperties.USE_LANDSCAPE_LAYOUT) {
            view.maybeCreateButtons(
                    model.get(HistorySyncProperties.USE_LANDSCAPE_LAYOUT),
                    model.get(HistorySyncProperties.MINOR_MODE_RESTRICTION_STATUS));
        } else {
            throw new IllegalArgumentException("Unknown property key: " + key);
        }
    }
}