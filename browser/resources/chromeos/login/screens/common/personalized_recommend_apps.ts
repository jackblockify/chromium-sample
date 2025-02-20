// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview Polymer element for perosonalized recommend apps screen.
 */

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/dialogs/oobe_loading_dialog.js';
import '../../components/oobe_personalized_apps_list.js';

import {assert} from '//resources/js/assert.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {MultiStepBehavior, MultiStepBehaviorInterface} from '../../components/behaviors/multi_step_behavior.js';
import {OobeDialogHostBehavior, OobeDialogHostBehaviorInterface} from '../../components/behaviors/oobe_dialog_host_behavior.js';
import {OobeUiState} from '../../components/display_manager_types.js';
import {OobeI18nMixin, OobeI18nMixinInterface} from '../../components/mixins/oobe_i18n_mixin.js';
import {CategoryAppsItems, OobePersonalizedAppsList} from '../../components/oobe_personalized_apps_list.js';

import {getTemplate} from './personalized_recommend_apps.html.js';

enum PersonalizedAppsStep {
  LOADING = 'loading',
  OVERVIEW = 'overview',
}

/**
 * Available user actions.
 */
enum UserAction {
  SKIP = 'skip',
  NEXT = 'next',
  BACK = 'back',
  LOADED = 'loaded',
}

export const PersonalizedRecommedAppsElementBase =
    mixinBehaviors(
        [LoginScreenBehavior, OobeDialogHostBehavior, MultiStepBehavior],
        OobeI18nMixin(PolymerElement)) as {
      new (): PolymerElement & OobeI18nMixinInterface &
          LoginScreenBehaviorInterface & OobeDialogHostBehaviorInterface &
          MultiStepBehaviorInterface,
    };

export class PersonalizedRecommedAppsElement extends
    PersonalizedRecommedAppsElementBase {
  static get is() {
    return 'personalized-apps-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      numberOfSelectedApps: {
        type: Number,
        value: 0,
      },
    };
  }

  private numberOfSelectedApps: number;

  override get UI_STEPS() {
    return PersonalizedAppsStep;
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep() {
    return PersonalizedAppsStep.LOADING;
  }

  override get EXTERNAL_API(): string[] {
    return [
      'setAppsAndUseCasesData',
      'setOverviewStep',
    ];
  }

  /**
   * Returns the control which should receive initial focus.
   */
  override get defaultControl(): HTMLElement|null {
    const categoriesDialog = this.shadowRoot?.querySelector<HTMLElement>(
        '#personalizedRecommendDialog');
    if (categoriesDialog instanceof HTMLElement) {
      return categoriesDialog;
    }
    return null;
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override getOobeUIInitialState(): OobeUiState {
    return OobeUiState.ONBOARDING;
  }

  override onBeforeShow(): void {
    this.setUIStep(PersonalizedAppsStep.LOADING);
  }

  onBeforeHide(): void {
    this.shadowRoot!
        .querySelector<OobePersonalizedAppsList>(
            '#categoriesAppsList')!.reset();
  }

  setAppsAndUseCasesData(categoriesData: CategoryAppsItems): void {
    assert(categoriesData !== null);
    this.shadowRoot!
        .querySelector<OobePersonalizedAppsList>('#categoriesAppsList')!.init(
            categoriesData);
  }

  setOverviewStep(): void {
    this.setUIStep(PersonalizedAppsStep.OVERVIEW);
    const categoriesAppsList =
        this.shadowRoot?.querySelector<HTMLElement>('#categoriesAppsList');
    if (categoriesAppsList instanceof HTMLElement) {
      categoriesAppsList.focus();
    }
  }

  /**
   * Handles event when contents in the webview is generated.
   */
  private onFullyLoaded(): void {
    this.userActed(UserAction.LOADED);
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('PersonalizedRecommendAppsScreen');
  }

  private onNextClicked(): void {
    const appsSelected = this.shadowRoot!
                             .querySelector<OobePersonalizedAppsList>(
                                 '#categoriesAppsList')!.getAppsSelected();
    this.userActed([UserAction.NEXT, appsSelected]);
  }

  private onSkip(): void {
    this.userActed(UserAction.SKIP);
  }

  private canProceed(): boolean {
    return this.numberOfSelectedApps > 0;
  }

  private onBackClicked(): void {
    this.userActed(UserAction.BACK);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [PersonalizedRecommedAppsElement.is]: PersonalizedRecommedAppsElement;
  }
}

customElements.define(
    PersonalizedRecommedAppsElement.is, PersonalizedRecommedAppsElement);
