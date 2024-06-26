// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-ax-annotations-subpage' is a subpage holding the toggle for main
 * node accessibility annotations. It appears on the accessibility page
 * (chrome://settings/accessibility) on Windows, macOS, and Linux.
 */

import '../controls/settings_toggle_button.js';
import '../settings_shared.css.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

import type {AccessibilityBrowserProxy} from './a11y_browser_proxy.js';
import {AccessibilityBrowserProxyImpl, ScreenAiInstallStatus} from './a11y_browser_proxy.js';
import {getTemplate} from './ax_annotations_subpage.html.js';

const SettingsAxAnnotationsSubpageBaseElement =
    PrefsMixin(WebUiListenerMixin(I18nMixin(PolymerElement)));

export class SettingsAxAnnotationsSubpageElement extends
    SettingsAxAnnotationsSubpageBaseElement {
  static get is() {
    return 'settings-ax-annotations-subpage';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },

      /**
       * `screenAIProgress_` stores the downloading progress in percentage of
       * the ScreenAI library, which ranges from 0.0 to 100.0.
       */
      screenAIProgress_: Number,

      /**
       * `screenAIStatus_` stores the ScreenAI library install state.
       */
      screenAIStatus_: Number,
    };
  }

  private browserProxy_: AccessibilityBrowserProxy =
      AccessibilityBrowserProxyImpl.getInstance();

  private showMainNodeAnnotationsToggle_: boolean;
  private screenAIProgress_: number;
  private screenAIStatus_: ScreenAiInstallStatus;

  override connectedCallback() {
    super.connectedCallback();

    assert(loadTimeData.getBoolean('mainNodeAnnotationsEnabled'));

    const updateScreenAIState = (screenAIState: ScreenAiInstallStatus) => {
      this.screenAIStatus_ = screenAIState;
    };
    this.browserProxy_.getScreenAiInstallState().then(updateScreenAIState);
    this.addWebUiListener('screen-ai-state-changed', updateScreenAIState);
    this.addWebUiListener(
        'screen-ai-downloading-progress-changed', (progress: number) => {
          this.screenAIProgress_ = progress;
        });
  }

  private getMainNodeAnnotationsToggleSublabel_(): string {
    switch (this.screenAIStatus_) {
      case ScreenAiInstallStatus.DOWNLOADING:
        return this.screenAIProgress_ > 0 && this.screenAIProgress_ < 100 ?
            this.i18n(
                'mainNodeAnnotationsDownloadProgressLabel',
                this.screenAIProgress_) :
            this.i18n('mainNodeAnnotationsDownloadingLabel');
      case ScreenAiInstallStatus.DOWNLOAD_FAILED:
        return this.i18n('mainNodeAnnotationsDownloadErrorLabel');
      case ScreenAiInstallStatus.DOWNLOADED:
        // Show the default subtitle if downloading is done.
        // fallthrough
      case ScreenAiInstallStatus.NOT_DOWNLOADED:
        // No subtitle update, so show a generic subtitle describing main node
        // annotations.
        return this.i18n('mainNodeAnnotationsSubtitle');
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-ax-annotations-subpage': SettingsAxAnnotationsSubpageElement;
  }
}

customElements.define(
    SettingsAxAnnotationsSubpageElement.is,
    SettingsAxAnnotationsSubpageElement);
