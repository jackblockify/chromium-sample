// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';
import '../../components/buttons/oobe_back_button.js';
import '../../components/buttons/oobe_next_button.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {OobeDialogHostBehavior, OobeDialogHostBehaviorInterface} from '../../components/behaviors/oobe_dialog_host_behavior.js';
import {OobeI18nMixin, OobeI18nMixinInterface} from '../../components/mixins/oobe_i18n_mixin.js';

import {getTemplate} from './placeholder.html.js';

const PlaceholderScreenElementBase = mixinBehaviors(
    [OobeDialogHostBehavior, LoginScreenBehavior],
    OobeI18nMixin(PolymerElement)) as {
      new (): PolymerElement & OobeI18nMixinInterface &
        OobeDialogHostBehaviorInterface & LoginScreenBehaviorInterface,
    };

enum UserAction {
  BACK = 'back',
  NEXT = 'next',
}

class PlaceholderScreen extends PlaceholderScreenElementBase {
  static get is() {
    return 'placeholder-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {};
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('PlaceholderScreen');
  }

  /**
   * Back button click handler.
   */
  private onBackClicked(): void {
    this.userActed(UserAction.BACK);
  }

  /**
   * Next button click handler.
   */
  private onNextClicked(): void {
    this.userActed(UserAction.NEXT);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [PlaceholderScreen.is]: PlaceholderScreen;
  }
}

customElements.define(PlaceholderScreen.is, PlaceholderScreen);
