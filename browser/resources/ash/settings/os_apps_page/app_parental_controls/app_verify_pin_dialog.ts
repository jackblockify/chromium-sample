// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'app-verify-pin-dialog' is the dialog for verifying that the PIN
 * a user enters to access App Parental Controls matches the existing PIN.
 */

import 'chrome://resources/ash/common/quick_unlock/pin_keyboard.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import '../../settings_shared.css.js';
import './app_setup_pin_keyboard.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PinKeyboardElement} from 'chrome://resources/ash/common/quick_unlock/pin_keyboard.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PIN_LENGTH} from './app_setup_pin_keyboard.js';
import {getTemplate} from './app_verify_pin_dialog.html.js';
import {ParentalControlsPinDialogError, recordPinDialogError} from './metrics_utils.js';

const AppVerifyPinDialogElementBase = PrefsMixin(I18nMixin(PolymerElement));

export interface AppVerifyPinDialogElement {
  $: {
    dialog: CrDialogElement,
    pinKeyboard: PinKeyboardElement,
  };
}

export class AppVerifyPinDialogElement extends AppVerifyPinDialogElementBase {
  static get is() {
    return 'app-verify-pin-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Whether verification of the entered PIN is in progress.
       * If true, the PIN keyboard input and confirm button should be disabled.
       */
      isVerificationPending_: {
        type: Boolean,
        value: false,
      },

      /**
       * The current PIN keyboard value.
       */
      pinValue_: {
        type: String,
        value: '',
      },

      /**
       * Whether the incorrect PIN error message should be displayed.
       */
      showError_: {
        type: Boolean,
        value: false,
      },
    };
  }

  private isVerificationPending_: boolean;
  private pinValue_: string;
  private showError_: boolean;

  override connectedCallback(): void {
    super.connectedCallback();

    this.resetState();
    this.$.dialog.showModal();
    this.$.pinKeyboard.focusInput();
  }

  close(): void {
    if (this.$.dialog.open) {
      this.$.dialog.close();
    }
    this.resetState();
  }

  resetState(): void {
    this.isVerificationPending_ = false;
    this.pinValue_ = '';
    this.showError_ = false;
  }

  private onCancelClick_(e: Event): void {
    // Stop propagation to keep the subpage from opening.
    e.stopPropagation();
    this.close();
  }

  private isConfirmDisabled_(): boolean {
    return this.isVerificationPending_ || this.pinValue_.length !== PIN_LENGTH;
  }

  /**
   * Save the most recently typed PIN when the user types or deletes a digit,
   * and hide a pre-existing error message if present.
   */
  private onPinChange_(event: CustomEvent<{pin: string}>): void {
    if (event && event.detail && event.detail.pin) {
      this.pinValue_ = event.detail.pin;
    }

    this.showError_ = false;
  }

  /**
   * Checks whether the PIN entered matches the saved PIN when the user presses
   * enter.
   */
  private onPinSubmit_(): void {
    this.isVerificationPending_ = true;

    if (this.pinValue_ &&
        this.pinValue_ === this.getPref('on_device_app_controls.pin').value) {
      // Trigger the method set in `on-pin-verified` by the containing dialog.
      this.dispatchEvent(new CustomEvent('pin-verified'));
      this.close();
    } else {
      // Focus the PIN keyboard so the user can re-attempt PIN entry.
      // Highlight the entire pin unless it is empty.
      this.showError_ = true;
      const length = this.pinValue_ ? this.pinValue_.length : 0;
      this.$.pinKeyboard.focusInput(0, length + 1);
      recordPinDialogError(ParentalControlsPinDialogError.INCORRECT_PIN);
    }

    this.isVerificationPending_ = false;
  }

  private onForgotPinClick_(): void {
    recordPinDialogError(ParentalControlsPinDialogError.FORGOT_PIN);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [AppVerifyPinDialogElement.is]: AppVerifyPinDialogElement;
  }
}

customElements.define(AppVerifyPinDialogElement.is, AppVerifyPinDialogElement);
