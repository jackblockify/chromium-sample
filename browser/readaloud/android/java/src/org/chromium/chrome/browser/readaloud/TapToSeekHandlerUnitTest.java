// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.modules.readaloud.Playback;
import org.chromium.chrome.modules.readaloud.Playback.PlaybackTextPart;
import org.chromium.chrome.modules.readaloud.Playback.PlaybackTextType;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link TapToSeekHandler} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({
    ChromeFeatureList.READALOUD,
    ChromeFeatureList.READALOUD_PLAYBACK,
    ChromeFeatureList.READALOUD_TAP_TO_SEEK
})
public class TapToSeekHandlerUnitTest {
    @Rule public JniMocker mJniMocker = new JniMocker();
    @Rule public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock private Profile mProfile;
    @Mock private ObservableSupplier<Tab> mMockTabProvider;
    private MockTab mTab;
    @Mock private Playback mPlayback;
    @Mock private Playback.Metadata mMetadata;
    private static final GURL sTestGURL = JUnitTestGURLs.EXAMPLE_URL;
    @Mock private ReadAloudFeatures.Natives mReadAloudFeaturesNatives;

    private TapToSeekHandler mTapToSeekHandler;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(false).when(mProfile).isOffTheRecord();
        mTab = new MockTab(1, mProfile);
        mTab.setGurlOverrideForTesting(sTestGURL);
        doReturn(mTab).when(mMockTabProvider).get();
        when(mPlayback.getMetadata()).thenReturn(mMetadata);
        mJniMocker.mock(ReadAloudFeaturesJni.TEST_HOOKS, mReadAloudFeaturesNatives);

        mTapToSeekHandler = new TapToSeekHandler(mMockTabProvider);
    }

    @After
    public void tearDown() {
        ReadAloudFeatures.shutdown();
    }

    @Test
    public void testTapToSeek_Successful() {
        // should match with the first case of 15 characters on either side
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        ReadAloudMetrics.HAS_TAP_TO_SEEK_FOUND_MATCH, true);
        tapToSeek("brown\nfox jumps", 6, 9, "The\nquick brown fox  jumps\nover the lazy dog.");
        histogram.assertExpected();
        verify(mPlayback, times(1)).seekToWord(0, 16);

        // should match with the second case of first half
        histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        ReadAloudMetrics.HAS_TAP_TO_SEEK_FOUND_MATCH, true);
        tapToSeek("over the lazy cat", 5, 13, "The\nquick brown fox  jumps\nover the lazy dog.");
        histogram.assertExpected();
        verify(mPlayback, times(1)).seekToWord(0, 32);

        // should match with the third case of second half
        histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        ReadAloudMetrics.HAS_TAP_TO_SEEK_FOUND_MATCH, true);
        tapToSeek("cat jumps  over", 4, 9, "The\nquick brown fox  jumps\nover the lazy dog.");
        histogram.assertExpected();
        verify(mPlayback, times(1)).seekToWord(0, 25);

        // removes parentheses
        histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        ReadAloudMetrics.HAS_TAP_TO_SEEK_FOUND_MATCH, true);
        tapToSeek(
                "The quick (remove me) brown [It should match] fox",
                4,
                9,
                "The\nquick brown fox  jumps\nover the lazy dog.");
        histogram.assertExpected();
        verify(mPlayback, times(1)).seekToWord(0, 8);
    }

    @Test
    public void testTapToSeek_Failure() {
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        ReadAloudMetrics.HAS_TAP_TO_SEEK_FOUND_MATCH, false);
        tapToSeek("grey\nmouse danced", 5, 10, "The\nquick brown fox  jumps\nover the lazy dog.");
        histogram.assertExpected();

        verify(mPlayback, never()).seekToWord(anyInt(), anyInt());
    }

    private void tapToSeek(String content, int beginOffset, int endOffset, String fullText) {
        // tap to seek
        when(mMetadata.fullText()).thenReturn(fullText);
        PlaybackTextPart p =
                new PlaybackTextPart() {
                    @Override
                    public int getOffset() {
                        return 0;
                    }

                    @Override
                    public int getType() {
                        return PlaybackTextType.TEXT_TYPE_UNSPECIFIED;
                    }

                    @Override
                    public int getParagraphIndex() {
                        return -1;
                    }

                    @Override
                    public int getLength() {
                        return -1;
                    }
                };
        PlaybackTextPart[] paragraphs = new PlaybackTextPart[] {p};
        when(mMetadata.paragraphs()).thenReturn(paragraphs);
        mTapToSeekHandler.tapToSeek(content, beginOffset, endOffset, mPlayback, mTab);
    }
}
