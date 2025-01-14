// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'additional-accounts-settings-card' is the card element containing the
 * accounts settings (including add / remove accounts).
 */

import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';
import 'chrome://resources/ash/common/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/ash/common/cr_elements/policy/cr_policy_indicator.js';
import 'chrome://resources/ash/common/cr_elements/policy/cr_tooltip_icon.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../settings_shared.css.js';

import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {assertInstanceof} from 'chrome://resources/js/assert.js';
import {getImage} from 'chrome://resources/js/icon.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {DomRepeat, DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertExists} from '../assert_extras.js';
import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {isChild} from '../common/load_time_booleans.js';
import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import {recordSettingChange} from '../metrics_recorder.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {Route, routes} from '../router.js';

import {Account, AccountManagerBrowserProxy, AccountManagerBrowserProxyImpl} from './account_manager_browser_proxy.js';
import {getTemplate} from './additional_accounts_settings_card.html.js';

const AdditionalAccountsSettingsCardElementBase = RouteObserverMixin(
    WebUiListenerMixin(I18nMixin(DeepLinkingMixin(PolymerElement))));

export interface AdditionalAccountsSettingsCardElement {
  $: {
    removeConfirmationDialog: CrDialogElement,
  };
}

export class AdditionalAccountsSettingsCardElement extends
    AdditionalAccountsSettingsCardElementBase {
  static get is() {
    return 'additional-accounts-settings-card' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      accounts: {
        type: Array,
        value() {
          return [];
        },
      },

      /**
       * The targeted account for menu operations.
       */
      actionMenuAccount_: Object,

      isChildUser_: {
        type: Boolean,
        value() {
          return isChild();
        },
        readOnly: true,
      },

      isDeviceAccountManaged_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isDeviceAccountManaged');
        },
        readOnly: true,
      },

      /**
       * @return true if secondary account sign-ins are allowed, false
       *  otherwise.
       */
      isSecondaryGoogleAccountSigninAllowed_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('secondaryGoogleAccountSigninAllowed');
        },
        readOnly: true,
      },

      /**
       * @return true if secondary account is allowed in ARC, false
       * otherwise
       */
      isSecondaryAccountAllowedInArc_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isSecondaryAccountAllowedInArc');
        },
        readOnly: true,
      },

      /**
       * @return true if `kArcAccountRestrictionsEnabled` feature is
       * enabled, false otherwise.
       */
      isArcAccountRestrictionsEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('arcAccountRestrictionsEnabled');
        },
        readOnly: true,
      },

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kAddAccount,
          Setting.kRemoveAccount,
        ]),
      },
    };
  }

  accounts: Account[];
  private actionMenuAccount_: Account|null;
  private browserProxy_: AccountManagerBrowserProxy;
  private isArcAccountRestrictionsEnabled_: boolean;
  private isChildUser_: boolean;
  private isDeviceAccountManaged_: boolean;
  private isSecondaryGoogleAccountSigninAllowed_: boolean;
  private readonly isSecondaryAccountAllowedInArc_: boolean;

  constructor() {
    super();

    this.browserProxy_ = AccountManagerBrowserProxyImpl.getInstance();
  }

  override currentRouteChanged(newRoute: Route): void {
    if (newRoute !== routes.OS_PEOPLE) {
      return;
    }

    this.attemptDeepLink();
  }

  /**
   * @return accounts list header (e.g. 'Secondary accounts' for
   * regular users or 'School accounts' for child users).
   */
  private getAccountListHeader_(): string {
    return this.isChildUser_ ? this.i18n('accountListHeaderChild') :
                               this.i18n('accountListHeader');
  }

  private getAccountListDescription_(): string {
    return this.isChildUser_ ? this.i18n('accountListChildDescription') :
                               this.i18n('accountListDescription');
  }

  private getSecondaryAccountsDisabledUserMessage_(): string {
    return this.isChildUser_ ?
        this.i18n('accountManagerSecondaryAccountsDisabledChildText') :
        this.i18n('accountManagerSecondaryAccountsDisabledText');
  }

  /**
   * @return a CSS image-set for multiple scale factors.
   */
  private getIconImageSet_(iconUrl: string): string {
    return getImage(iconUrl);
  }

  private addAccount_(): void {
    recordSettingChange(
        Setting.kAddAccount, {intValue: this.accounts.length + 1});
    this.browserProxy_.addAccount();
  }

  private shouldShowReauthenticationButton_(account: Account): boolean {
    // Device account re-authentication cannot be handled in-session, primarily
    // because the user may have changed their password (leading to an LST
    // invalidation) and we do not have a mechanism to change the cryptohome
    // password in-session.
    return !account.isDeviceAccount && !account.isSignedIn;
  }

  private getManagedAccountTooltipIcon_(): string {
    if (this.isChildUser_) {
      return 'cr20:kite';
    }
    if (this.isDeviceAccountManaged_) {
      return 'cr20:domain';
    }
    return '';
  }

  private getAccountManagerSignedOutName_(unmigrated: boolean): string {
    return this.i18n(
        unmigrated ? 'accountManagerUnmigratedAccountName' :
                     'accountManagerSignedOutAccountName');
  }

  private getAccountManagerSignedOutLabel_(unmigrated: boolean): string {
    return this.i18n(
        unmigrated ? 'accountManagerMigrationLabel' :
                     'accountManagerReauthenticationLabel');
  }

  private getAccountManagerSignedOutTitle_(account: Account): string {
    const label = account.unmigrated ? 'accountManagerMigrationTooltip' :
                                       'accountManagerReauthenticationTooltip';
    return this.i18n(label, account.email);
  }

  private getMoreActionsTitle_(account: Account): string {
    return this.i18n('accountManagerMoreActionsTooltip', account.email);
  }

  private shouldShowSecondaryAccountsList_(): boolean {
    return this.getSecondaryAccounts_().length === 0;
  }

  private getSecondaryAccounts_(): Account[] {
    return this.accounts.filter(account => !account.isDeviceAccount);
  }

  private getAddAccountLabel_(): string {
    if (this.isChildUser_ && this.isSecondaryGoogleAccountSigninAllowed_) {
      return this.i18n('addSchoolAccountLabel');
    }
    return this.i18n('addAccountLabel');
  }

  private onReauthenticationClick_(event: DomRepeatEvent<Account>): void {
    if (event.model.item.unmigrated) {
      this.browserProxy_.migrateAccount(event.model.item.email);
    } else {
      this.browserProxy_.reauthenticateAccount(event.model.item.email);
    }
  }

  private onAccountActionsMenuButtonClick_(event: DomRepeatEvent<Account>):
      void {
    this.actionMenuAccount_ = event.model.item;

    assertInstanceof(event.target, HTMLElement);
    this.shadowRoot!.querySelector('cr-action-menu')!.showAt(event.target);
  }

  /**
   * If Lacros is not enabled, removes the account pointed to by
   * |this.actionMenuAccount_|.
   * If Lacros is enabled, shows a warning dialog that the user needs to
   * confirm before removing the account.
   */
  private onRemoveAccountClick_(): void {
    this.shadowRoot!.querySelector('cr-action-menu')!.close();
    assertExists(this.actionMenuAccount_);
    if (loadTimeData.getBoolean('lacrosEnabled') &&
        this.actionMenuAccount_.isManaged) {
      this.$.removeConfirmationDialog.showModal();
    } else {
      this.browserProxy_.removeAccount(this.actionMenuAccount_);
      this.actionMenuAccount_ = null;
      this.shadowRoot!.querySelector<CrButtonElement>(
                          '#addAccountButton')!.focus();
    }
  }

  /**
   * The user chooses not to remove the account after seeing the warning
   * dialog, and taps the cancel button.
   */
  private onRemoveAccountDialogCancelClick_(): void {
    this.actionMenuAccount_ = null;
    this.$.removeConfirmationDialog.cancel();
    this.shadowRoot!.querySelector<CrButtonElement>(
                        '#addAccountButton')!.focus();
  }

  /**
   * After seeing the warning dialog, the user chooses to removes the account
   * pointed to by |this.actionMenuAccount_|, and taps the remove button.
   */
  private onRemoveAccountDialogRemoveClick_(): void {
    assertExists(this.actionMenuAccount_);
    this.browserProxy_.removeAccount(this.actionMenuAccount_);
    this.actionMenuAccount_ = null;
    this.$.removeConfirmationDialog.close();
    this.shadowRoot!.querySelector<CrButtonElement>(
                        '#addAccountButton')!.focus();
  }

  /**
   * Get the test for button that changes ARC availability.
   */
  private getChangeArcAvailabilityLabel_(): string {
    if (!this.actionMenuAccount_) {
      return '';
    }

    // For a managed account whenever SecondaryAccountAllowedInArcPolicy
    // is false, it should always show "Use with Android apps".
    // Other times the label is dependent on isAvailableInArc's value
    if (this.isSecondaryAccountInAndroidBlockedByPolicy_()) {
      return this.i18n('accountUseInArcButtonLabel');
    }

    return this.actionMenuAccount_.isAvailableInArc ?
        this.i18n('accountStopUsingInArcButtonLabel') :
        this.i18n('accountUseInArcButtonLabel');
  }

  /**
   * Only for managed accounts, this checks if
   * SecondaryAccountAllowedInArcPolicy is blocking the account from being used
   * as a secondary account with Android apps.
   */
  private isSecondaryAccountInAndroidBlockedByPolicy_(): boolean {
    if (!this.actionMenuAccount_) {
      return false;
    }

    return this.actionMenuAccount_.isManaged &&
        !this.isSecondaryAccountAllowedInArc_;
  }

  /**
   * If SecondaryAccountAllowedInArcPolicy blocks the managed account added as
   * secondary account to be used with Android apps, it should display tooltip
   * 'Not used with Android apps'.
   */
  private displayArcAvailabilityStatus_(account: Account): boolean {
    if (account.isManaged && !this.isSecondaryAccountAllowedInArc_) {
      return false;
    }
    return account.isAvailableInArc;
  }

  /**
   * Change ARC availability for |this.actionMenuAccount_|.
   * Closes the 'More actions' menu and focuses the 'More actions' button for
   * |this.actionMenuAccount_|.
   */
  private onChangeArcAvailability_(): void {
    assertExists(this.actionMenuAccount_);
    this.shadowRoot!.querySelector('cr-action-menu')!.close();
    const newArcAvailability = !this.actionMenuAccount_.isAvailableInArc;
    this.browserProxy_.changeArcAvailability(
        this.actionMenuAccount_, newArcAvailability);

    const actionMenuAccountIndex =
        this.shadowRoot!.querySelector<DomRepeat>('#secondaryAccountsList')!
            .items!.indexOf(this.actionMenuAccount_);
    if (actionMenuAccountIndex >= 0) {
      // Focus 'More actions' button for the current account.
      this.shadowRoot!
          .querySelectorAll<HTMLElement>(
              '.icon-more-vert')[actionMenuAccountIndex]
          .focus();
    } else {
      console.error(
          'Couldn\'t find active account in the list: ',
          this.actionMenuAccount_);
      this.shadowRoot!.querySelector<CrButtonElement>(
                          '#addAccountButton')!.focus();
    }
    this.actionMenuAccount_ = null;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [AdditionalAccountsSettingsCardElement.is]:
        AdditionalAccountsSettingsCardElement;
  }
}

customElements.define(
    AdditionalAccountsSettingsCardElement.is,
    AdditionalAccountsSettingsCardElement);
