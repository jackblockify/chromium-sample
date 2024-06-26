// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.scrollTo;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.CoreMatchers.not;
import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.app.Dialog;
import android.content.Context;
import android.os.Bundle;
import android.view.View;

import androidx.test.espresso.PerformException;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matcher;
import org.junit.After;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;

/** Tests {@link PrivacySandboxDialog}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public final class PrivacySandboxDialogTest {
    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.UI_SETTINGS_PRIVACY)
                    .build();

    @Rule public JniMocker mocker = new JniMocker();

    private FakePrivacySandboxBridge mFakePrivacySandboxBridge;

    @Mock private SettingsLauncher mSettingsLauncher;

    private Dialog mDialog;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mFakePrivacySandboxBridge = new FakePrivacySandboxBridge();
        mocker.mock(PrivacySandboxBridgeJni.TEST_HOOKS, mFakePrivacySandboxBridge);
        PrivacySandboxDialogController.disableAnimationsForTesting(true);
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Dismiss the dialog between the tests. Necessary due to batching.
                    if (mDialog != null) {
                        mDialog.dismiss();
                        mDialog = null;
                    }
                });
    }

    private void renderViewWithId(int id, String renderId) {
        onViewWaiting(withId(id), true);
        onView(withId(id))
                .inRoot(isDialog())
                .check(
                        (v, noMatchException) -> {
                            if (noMatchException != null) throw noMatchException;
                            try {
                                TestThreadUtils.runOnUiThreadBlocking(
                                        () -> RenderTestRule.sanitize(v));
                                mRenderTestRule.render(v, renderId);
                            } catch (IOException e) {
                                assert false : "Render test failed due to " + e;
                            }
                        });
    }

    private void launchDialog() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (mDialog != null) {
                        mDialog.dismiss();
                        mDialog = null;
                    }
                    PrivacySandboxDialogController.maybeLaunchPrivacySandboxDialog(
                            sActivityTestRule.getActivity(),
                            mSettingsLauncher,
                            sActivityTestRule.getProfile(false));
                    mDialog = PrivacySandboxDialogController.getDialogForTesting();
                });
    }

    private void tryClickOn(Matcher<View> viewMatcher) {
        clickMoreButtonUntilFullyScrolledDown();
        onViewWaiting(viewMatcher, true).perform(click());
    }

    private void clickMoreButtonUntilFullyScrolledDown() {
        while (true) {
            try {
                onView(withId(R.id.more_button)).inRoot(isDialog()).perform(click());
                var promptType = mFakePrivacySandboxBridge.getRequiredPromptType();
                if (promptType == PromptType.M1_CONSENT) {
                    assertEquals(
                            "Last dialog action",
                            PromptAction.CONSENT_MORE_BUTTON_CLICKED,
                            (int) mFakePrivacySandboxBridge.getLastPromptAction());
                } else if (promptType == PromptType.M1_NOTICE_EEA
                        || promptType == PromptType.M1_NOTICE_ROW) {
                    assertEquals(
                            "Last dialog action",
                            PromptAction.NOTICE_MORE_BUTTON_CLICKED,
                            (int) mFakePrivacySandboxBridge.getLastPromptAction());
                } else if (promptType == PromptType.M1_NOTICE_RESTRICTED) {
                    assertEquals(
                            "Last dialog action",
                            PromptAction.RESTRICTED_NOTICE_MORE_BUTTON_CLICKED,
                            (int) mFakePrivacySandboxBridge.getLastPromptAction());
                }
            } catch (PerformException e) {
                return;
            }
        }
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderEEAConsent() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mDialog =
                            new PrivacySandboxDialogConsentEEA(
                                    sActivityTestRule.getActivity(),
                                    new PrivacySandboxBridge(sActivityTestRule.getProfile(false)),
                                    /* animate= */ mSettingsLauncher,
                                    false);
                    mDialog.show();
                });
        renderViewWithId(R.id.privacy_sandbox_dialog, "privacy_sandbox_eea_consent_dialog");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderEEANotice() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mDialog =
                            new PrivacySandboxDialogNoticeEEA(
                                    sActivityTestRule.getActivity(),
                                    new PrivacySandboxBridge(sActivityTestRule.getProfile(false)),
                                    mSettingsLauncher);
                    mDialog.show();
                });
        renderViewWithId(R.id.privacy_sandbox_dialog, "privacy_sandbox_eea_notice_dialog");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderROWNotice() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mDialog =
                            new PrivacySandboxDialogNoticeROW(
                                    sActivityTestRule.getActivity(),
                                    new PrivacySandboxBridge(sActivityTestRule.getProfile(false)),
                                    mSettingsLauncher);
                    mDialog.show();
                });
        renderViewWithId(R.id.privacy_sandbox_dialog, "privacy_sandbox_row_notice_dialog");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testRenderRestrictedNotice() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mDialog =
                            new PrivacySandboxDialogNoticeRestricted(
                                    sActivityTestRule.getActivity(),
                                    new PrivacySandboxBridge(sActivityTestRule.getProfile(false)),
                                    mSettingsLauncher);
                    mDialog.show();
                });
        renderViewWithId(R.id.privacy_sandbox_dialog, "privacy_sandbox_restricted_notice_dialog");
    }

    @Test
    @SmallTest
    public void testControllerIncognito() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PrivacySandboxDialogController.maybeLaunchPrivacySandboxDialog(
                            sActivityTestRule.getActivity(),
                            mSettingsLauncher,
                            sActivityTestRule.getProfile(true));
                });
        // Verify that nothing is shown.
        onView(withId(R.id.privacy_sandbox_dialog)).check(doesNotExist());
    }

    @Test
    @SmallTest
    public void testControllerShowsNothing() throws IOException {
        mFakePrivacySandboxBridge.setRequiredPromptType(PromptType.NONE);
        launchDialog();
        // Verify that nothing is shown. Notice & Consent share a title.
        onView(withId(R.id.privacy_sandbox_dialog)).check(doesNotExist());
    }

    @Test
    @SmallTest
    public void testControllerShowsEEAConsent() throws IOException {
        PrivacySandboxDialogController.disableEEANoticeForTesting(true);

        mFakePrivacySandboxBridge.setRequiredPromptType(PromptType.M1_CONSENT);
        launchDialog();

        // Verify that the EEA consent is shown
        onViewWaiting(withId(R.id.privacy_sandbox_m1_consent_title), true);
        assertEquals(
                "Last dialog action",
                PromptAction.CONSENT_SHOWN,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        // Accept the consent and verify it worked correctly.
        tryClickOn(withId(R.id.ack_button));
        assertEquals(
                "Last dialog action",
                PromptAction.CONSENT_ACCEPTED,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        onView(withId(R.id.privacy_sandbox_consent_eea_dropdown)).check(doesNotExist());
    }

    @Test
    @SmallTest
    public void testControllerShowsEEAConsentDropdown() {
        PrivacySandboxDialogController.disableEEANoticeForTesting(true);

        mFakePrivacySandboxBridge.setRequiredPromptType(PromptType.M1_CONSENT);
        launchDialog();

        // Click on the expanding section and verify it worked correctly.
        onViewWaiting(withId(R.id.privacy_sandbox_m1_consent_title), true);
        onView(withId(R.id.dropdown_element)).inRoot(isDialog()).perform(scrollTo(), click());
        assertEquals(
                "Last dialog action",
                PromptAction.CONSENT_MORE_INFO_OPENED,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());

        onView(withId(R.id.privacy_sandbox_consent_eea_dropdown)).inRoot(isDialog()).perform(scrollTo());
        onView(withId(R.id.privacy_sandbox_consent_eea_dropdown)).inRoot(isDialog()).check(matches(isDisplayed()));
        onView(withId(R.id.dropdown_element)).inRoot(isDialog()).perform(scrollTo(), click());
        assertEquals(
                "Last dialog action",
                PromptAction.CONSENT_MORE_INFO_CLOSED,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        onView(withId(R.id.privacy_sandbox_consent_eea_dropdown)).inRoot(isDialog()).check(doesNotExist());

        // Decline the consent and verify it worked correctly.
        tryClickOn(withId(R.id.no_button));
        assertEquals(
                "Last dialog action",
                PromptAction.CONSENT_DECLINED,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        onView(withId(R.id.privacy_sandbox_consent_eea_dropdown)).check(doesNotExist());
    }

    @Test
    @SmallTest
    public void testAfterEEAConsentSpinnerAndNoticeAreShown() throws IOException {
        PrivacySandboxDialogController.disableAnimationsForTesting(false);

        // Launch the consent
        mFakePrivacySandboxBridge.setRequiredPromptType(PromptType.M1_CONSENT);
        launchDialog();

        // Accept the consent and verify the spinner it's shown.
        tryClickOn(withId(R.id.ack_button));
        onViewWaiting(withId(R.id.privacy_sandbox_m1_consent_title), true)
                .check(matches(not(isDisplayed())));

        onView(withId(R.id.progress_bar_container))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));

        // Wait for the spinner to disappear and check the notice is shown
        onViewWaiting(withId(R.id.privacy_sandbox_notice_title), true)
                .check(matches(isDisplayed()));

        onView(withId(R.id.privacy_sandbox_m1_consent_title))
                .inRoot(isDialog())
                .check(doesNotExist());
        onView(withId(R.id.progress_bar_container)).inRoot(isDialog()).check(doesNotExist());

        // Launch the consent
        launchDialog();

        // Decline the consent and verify the spinner it's shown.
        tryClickOn(withId(R.id.no_button));
        onViewWaiting(withId(R.id.privacy_sandbox_m1_consent_title), true)
                .check(matches(not(isDisplayed())));

        onView(withId(R.id.progress_bar_container))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));

        // Wait for the spinner to disappear and check the notice is shown
        onViewWaiting(withId(R.id.privacy_sandbox_notice_title), true)
                .check(matches(isDisplayed()));
        onView(withId(R.id.privacy_sandbox_m1_consent_title))
                .inRoot(isDialog())
                .check(doesNotExist());
        onView(withId(R.id.progress_bar_container)).inRoot(isDialog()).check(doesNotExist());
    }

    @Test
    @SmallTest
    public void testControllerShowsEEANotice() throws IOException {
        mFakePrivacySandboxBridge.setRequiredPromptType(PromptType.M1_NOTICE_EEA);
        launchDialog();
        // Verify that the EEA notice is shown
        onViewWaiting(withId(R.id.privacy_sandbox_notice_title), true);
        assertEquals(
                "Last dialog action",
                PromptAction.NOTICE_SHOWN,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        // Ack the notice and verify it worked correctly.
        tryClickOn(withId(R.id.ack_button));
        assertEquals(
                "Last dialog action",
                PromptAction.NOTICE_ACKNOWLEDGE,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        onView(withId(R.id.privacy_sandbox_notice_title)).check(doesNotExist());

        launchDialog();
        // Click on the expanding section and verify it worked correctly.
        onViewWaiting(withId(R.id.privacy_sandbox_notice_title), true);
        onView(withId(R.id.dropdown_element)).inRoot(isDialog()).perform(scrollTo(), click());
        assertEquals(
                "Last dialog action",
                PromptAction.NOTICE_MORE_INFO_OPENED,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());

        onView(withId(R.id.privacy_sandbox_notice_eea_dropdown))
                .inRoot(isDialog())
                .perform(scrollTo());
        onView(withId(R.id.privacy_sandbox_notice_eea_dropdown))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
        onView(withId(R.id.dropdown_element)).inRoot(isDialog()).perform(scrollTo(), click());
        assertEquals(
                "Last dialog action",
                PromptAction.NOTICE_MORE_INFO_CLOSED,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        onView(withId(R.id.privacy_sandbox_notice_eea_dropdown)).check(doesNotExist());

        // Click on the settings button and verify it worked correctly.
        tryClickOn(withId(R.id.settings_button));
        onView(withId(R.id.privacy_sandbox_notice_title)).check(doesNotExist());
        assertEquals(
                "Last dialog action",
                PromptAction.NOTICE_OPEN_SETTINGS,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        Mockito.verify(mSettingsLauncher)
                .launchSettingsActivity(
                        any(Context.class),
                        eq(PrivacySandboxSettingsFragment.class),
                        any(Bundle.class));
    }

    @Test
    @SmallTest
    public void testControllerShowsROWNotice() throws IOException {
        mFakePrivacySandboxBridge.setRequiredPromptType(PromptType.M1_NOTICE_ROW);
        launchDialog();
        // Verify that the ROW notice is shown
        onViewWaiting(withId(R.id.privacy_sandbox_notice_title), true);
        assertEquals(
                "Last dialog action",
                PromptAction.NOTICE_SHOWN,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        // Ack the notice and verify it worked correctly.
        tryClickOn(withId(R.id.ack_button));
        assertEquals(
                "Last dialog action",
                PromptAction.NOTICE_ACKNOWLEDGE,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        onView(withId(R.id.privacy_sandbox_notice_title)).check(doesNotExist());

        launchDialog();
        // Click on the expanding section and verify it worked correctly.
        onViewWaiting(withId(R.id.privacy_sandbox_notice_title), true);
        onView(withId(R.id.dropdown_element)).inRoot(isDialog()).perform(scrollTo(), click());
        assertEquals(
                "Last dialog action",
                PromptAction.NOTICE_MORE_INFO_OPENED,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());

        onView(withId(R.id.privacy_sandbox_notice_row_dropdown)).inRoot(isDialog()).check(matches(isDisplayed()));
        onView(withId(R.id.dropdown_element)).inRoot(isDialog()).perform(scrollTo(), click());
        assertEquals(
                "Last dialog action",
                PromptAction.NOTICE_MORE_INFO_CLOSED,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        onView(withId(R.id.privacy_sandbox_notice_row_dropdown)).inRoot(isDialog()).check(doesNotExist());

        // Click on the settings button and verify it worked correctly.
        tryClickOn(withId(R.id.settings_button));
        assertEquals(
                "Last dialog action",
                PromptAction.NOTICE_OPEN_SETTINGS,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        onView(withId(R.id.privacy_sandbox_notice_title)).check(doesNotExist());
        Mockito.verify(mSettingsLauncher)
                .launchSettingsActivity(
                        any(Context.class),
                        eq(PrivacySandboxSettingsFragment.class),
                        any(Bundle.class));
    }

    @Test
    @SmallTest
    public void testControllerShowsRestrictedNotice() throws IOException {
        mFakePrivacySandboxBridge.setRequiredPromptType(PromptType.M1_NOTICE_RESTRICTED);
        launchDialog();
        // Verify that the restricted notice is shown
        onViewWaiting(withId(R.id.privacy_sandbox_notice_title), true);
        assertEquals(
                "Last dialog action",
                PromptAction.RESTRICTED_NOTICE_SHOWN,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        // Ack the notice and verify it worked correctly.
        tryClickOn(withId(R.id.ack_button));
        assertEquals(
                "Last dialog action",
                PromptAction.RESTRICTED_NOTICE_ACKNOWLEDGE,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        onView(withId(R.id.privacy_sandbox_notice_title)).check(doesNotExist());

        // Click on the settings button and verify it worked correctly.
        launchDialog();
        tryClickOn(withId(R.id.settings_button));
        assertEquals(
                "Last dialog action",
                PromptAction.RESTRICTED_NOTICE_OPEN_SETTINGS,
                (int) mFakePrivacySandboxBridge.getLastPromptAction());
        onView(withId(R.id.privacy_sandbox_notice_title)).check(doesNotExist());
        Mockito.verify(mSettingsLauncher)
                .launchSettingsActivity(any(Context.class), eq(AdMeasurementFragment.class));
    }
}
