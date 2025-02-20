// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.mini;

import android.app.Activity;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.ViewStub;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.readaloud.ReadAloudMiniPlayerSceneLayer;
import org.chromium.chrome.browser.readaloud.player.PlayerCoordinator;
import org.chromium.chrome.browser.readaloud.player.R;
import org.chromium.chrome.browser.readaloud.player.VisibilityState;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator responsible for Read Aloud mini player lifecycle. */
public class MiniPlayerCoordinator {
    private static ViewStub sViewStubForTesting;
    private final PropertyModelChangeProcessor<PropertyModel, MiniPlayerLayout, PropertyKey>
            mPlayerModelChangeProcessor;
    private final PropertyModelChangeProcessor<
                    PropertyModel, MiniPlayerViewBinder.ViewHolder, PropertyKey>
            mMiniPlayerModelChangeProcessor;
    private final MiniPlayerMediator mMediator;
    private final MiniPlayerLayout mLayout;
    // Compositor layer to be shown during show and hide while browser controls are
    // resizing.
    private final ReadAloudMiniPlayerSceneLayer mSceneLayer;
    private final PlayerCoordinator mPlayerCoordinator;

    /**
     * @param activity App activity containing a placeholder FrameLayout with ID
     *     R.id.readaloud_mini_player.
     * @param context View-inflation-capable Context for read_aloud_playback isolated split.
     * @param sharedModel Player UI property model for properties shared with expanded player.
     * @param bottomControlsStacker Allows observing and changing browser controls heights.
     * @param layoutManager Involved in showing the compositor view.
     * @param playerCoordinator PlayerCoordinator to be notified of mini player updates.
     */
    public MiniPlayerCoordinator(
            Activity activity,
            Context context,
            PropertyModel sharedModel,
            BottomControlsStacker bottomControlsStacker,
            @Nullable LayoutManager layoutManager,
            PlayerCoordinator playerCoordinator) {
        this(
                sharedModel,
                new MiniPlayerMediator(bottomControlsStacker),
                inflateLayout(activity, context),
                new ReadAloudMiniPlayerSceneLayer(bottomControlsStacker.getBrowserControls()),
                layoutManager,
                playerCoordinator);
    }

    private static MiniPlayerLayout inflateLayout(Activity activity, Context context) {
        ViewStub stub =
                sViewStubForTesting != null
                        ? sViewStubForTesting
                        : activity.findViewById(R.id.readaloud_mini_player_stub);
        assert stub != null;
        stub.setLayoutResource(R.layout.readaloud_mini_player_layout);
        stub.setLayoutInflater(LayoutInflater.from(context));
        return (MiniPlayerLayout) stub.inflate();
    }

    @VisibleForTesting
    MiniPlayerCoordinator(
            PropertyModel sharedModel,
            MiniPlayerMediator mediator,
            MiniPlayerLayout layout,
            ReadAloudMiniPlayerSceneLayer sceneLayer,
            @Nullable LayoutManager layoutManager,
            PlayerCoordinator playerCoordinator) {
        mMediator = mediator;
        mMediator.setCoordinator(this);
        mLayout = layout;
        assert layout != null;
        mSceneLayer = sceneLayer;
        sceneLayer.setIsVisible(true);
        if (layoutManager != null) {
            layoutManager.addSceneOverlay(sceneLayer);
        }
        mPlayerCoordinator = playerCoordinator;

        mPlayerModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        sharedModel, mLayout, MiniPlayerViewBinder::bindPlayerProperties);
        mMiniPlayerModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mMediator.getModel(),
                        new MiniPlayerViewBinder.ViewHolder(layout, sceneLayer),
                        MiniPlayerViewBinder::bindMiniPlayerProperties);
    }

    public void destroy() {
        mLayout.destroy();
    }

    /**
     * Show the mini player if it isn't already showing.
     * @param animate True if the transition should be animated. If false, the mini player will
     *         instantly appear.
     */
    public void show(boolean animate) {
        mMediator.show(animate);
    }

    /** Returns the mini player visibility state. */
    public @VisibilityState int getVisibility() {
        return mMediator.getVisibility();
    }

    /**
     * Dismiss the mini player.
     *
     * @param animate True if the transition should be animated. If false, the mini
     *                player will
     *                instantly disappear (though web contents resizing may lag
     *                behind).
     */
    public void dismiss(boolean animate) {
        mMediator.dismiss(animate);
    }

    void onShown() {
        mPlayerCoordinator.onMiniPlayerShown();
    }

    public static void setViewStubForTesting(ViewStub stub) {
        sViewStubForTesting = stub;
    }
}
