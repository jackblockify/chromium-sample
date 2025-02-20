// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';
import '../../components/dialogs/oobe_loading_dialog.js';
import '../../components/buttons/oobe_back_button.js';
import '../../components/buttons/oobe_next_button.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {MultiStepBehavior, MultiStepBehaviorInterface} from '../../components/behaviors/multi_step_behavior.js';
import {OobeDialogHostBehavior, OobeDialogHostBehaviorInterface} from '../../components/behaviors/oobe_dialog_host_behavior.js';
import {OobeI18nMixin, OobeI18nMixinInterface} from '../../components/mixins/oobe_i18n_mixin.js';

import {getTemplate} from './password_selection.html.js';

/**
 * Type of the password for setting up for the user.
 */
enum PasswordType {
  LOCAL_PASSWORD = 'local-password',
  GAIA_PASSWORD = 'gaia-password',
}

/**
 * UI mode for the dialog.
 */
enum PasswordSelectionState {
  SELECTION = 'selection',
  PROGRESS = 'progress',
}

const PasswordSelectionBase = mixinBehaviors(
                                         [
                                           OobeDialogHostBehavior,
                                           LoginScreenBehavior,
                                           MultiStepBehavior,
                                         ],
                                         OobeI18nMixin(PolymerElement)) as {
  new (): PolymerElement & OobeDialogHostBehaviorInterface &
      OobeI18nMixinInterface & LoginScreenBehaviorInterface &
      MultiStepBehaviorInterface,
};

export class PasswordSelection extends PasswordSelectionBase {
  static get is() {
    return 'password-selection-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * The currently selected password type.
       */
      selectedPasswordType: {
        type: String,
      },

      /**
       * Enum values for `selectedPasswordType`.
       *  {PasswordType}
       */
      passwordTypeEnum: {
        readOnly: true,
        type: Object,
        value: PasswordType,
      },
    };
  }

  private selectedPasswordType: string;
  private passwordTypeEnum: PasswordType;

  override ready(): void {
    super.ready();

    this.initializeLoginScreen('PasswordSelectionScreen');
  }
  override get EXTERNAL_API(): string[] {
    return [
      'showProgress',
      'showPasswordChoice',
    ];
  }

  override get UI_STEPS() {
    return PasswordSelectionState;
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep(): PasswordSelectionState {
    return PasswordSelectionState.PROGRESS;
  }

  // Invoked just before being shown. Contains all the data for the screen.
  override onBeforeShow(): void {
    this.selectedPasswordType = PasswordType.LOCAL_PASSWORD;
  }

  showProgress(): void {
    this.setUIStep(PasswordSelectionState.PROGRESS);
  }

  showPasswordChoice(): void {
    this.setUIStep(PasswordSelectionState.SELECTION);
  }

  private onBackClicked(): void {
    this.userActed('back');
  }

  private onNextClicked(): void {
    this.userActed(this.selectedPasswordType);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [PasswordSelection.is]: PasswordSelection;
  }
}

customElements.define(PasswordSelection.is, PasswordSelection);
