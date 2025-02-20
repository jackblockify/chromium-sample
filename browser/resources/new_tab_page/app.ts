// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './iframe.js';
import './logo.js';
import './strings.m.js';
import 'chrome://resources/cr_components/searchbox/realbox.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';

import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import type {HelpBubbleMixinInterface} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin.js';
import {HelpBubbleMixin} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin.js';
import type {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import type {ClickInfo} from 'chrome://resources/js/browser_command.mojom-webui.js';
import {Command} from 'chrome://resources/js/browser_command.mojom-webui.js';
import {BrowserCommandProxy} from 'chrome://resources/js/browser_command/browser_command_proxy.js';
import {hexColorToSkColor, skColorToRgba} from 'chrome://resources/js/color_utils.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {FocusOutlineManager} from 'chrome://resources/js/focus_outline_manager.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getTrustedScriptURL} from 'chrome://resources/js/static_types.js';
import type {SkColor} from 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';
import type {DomIf} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import {BackgroundManager} from './background_manager.js';
import {CustomizeDialogPage} from './customize_dialog_types.js';
import type {IframeElement} from './iframe.js';
import type {LogoElement} from './logo.js';
import {recordDuration, recordLoadDuration} from './metrics_utils.js';
import type {PageCallbackRouter, PageHandlerRemote, Theme} from './new_tab_page.mojom-webui.js';
import {CustomizeChromeSection, IphFeature, NtpBackgroundImageSource} from './new_tab_page.mojom-webui.js';
import {NewTabPageProxy} from './new_tab_page_proxy.js';
import {$$} from './utils.js';
import {Action as VoiceAction, recordVoiceAction} from './voice_search_overlay.js';
import {WindowProxy} from './window_proxy.js';

interface ExecutePromoBrowserCommandData {
  commandId: Command;
  clickInfo: ClickInfo;
}

interface CanShowPromoWithBrowserCommandData {
  frameType: string;
  messageType: string;
  commandId: Command;
}

/**
 * Elements on the NTP. This enum must match the numbering for NTPElement in
 * enums.xml. These values are persisted to logs. Entries should not be
 * renumbered, removed or reused.
 */
export enum NtpElement {
  OTHER = 0,
  BACKGROUND = 1,
  ONE_GOOGLE_BAR = 2,
  LOGO = 3,
  REALBOX = 4,
  MOST_VISITED = 5,
  MIDDLE_SLOT_PROMO = 6,
  MODULE = 7,
  CUSTOMIZE = 8,  // Obsolete
  CUSTOMIZE_BUTTON = 9,
  CUSTOMIZE_DIALOG = 10,
  WALLPAPER_SEARCH_BUTTON = 11,
  MAX_VALUE = WALLPAPER_SEARCH_BUTTON,
}

/**
 * Customize Chrome entry points. This enum must match the numbering for
 * NtpCustomizeChromeEntryPoint in enums.xml. These values are persisted to
 * logs. Entries should not be renumbered, removed or reused.
 */
export enum NtpCustomizeChromeEntryPoint {
  CUSTOMIZE_BUTTON = 0,
  MODULE = 1,
  URL = 2,
  WALLPAPER_SEARCH_BUTTON = 3,
  MAX_VALUE = WALLPAPER_SEARCH_BUTTON,
}

const CUSTOMIZE_URL_PARAM: string = 'customize';
const OGB_IFRAME_ORIGIN = 'chrome-untrusted://new-tab-page';

export const CUSTOMIZE_CHROME_BUTTON_ELEMENT_ID =
    'NewTabPageUI::kCustomizeChromeButtonElementId';

// 900px ~= 561px (max value for --ntp-search-box-width) * 1.5 + some margin.
const realboxCanShowSecondarySideMediaQueryList =
    window.matchMedia('(min-width: 900px)');

function recordClick(element: NtpElement) {
  chrome.metricsPrivate.recordEnumerationValue(
      'NewTabPage.Click', element, NtpElement.MAX_VALUE + 1);
}

function recordCustomizeChromeOpen(element: NtpCustomizeChromeEntryPoint) {
  chrome.metricsPrivate.recordEnumerationValue(
      'NewTabPage.CustomizeChromeOpened', element,
      NtpCustomizeChromeEntryPoint.MAX_VALUE + 1);
}

// Adds a <script> tag that holds the lazy loaded code.
function ensureLazyLoaded() {
  const script = document.createElement('script');
  script.type = 'module';
  script.src = getTrustedScriptURL`./lazy_load.js`;
  document.body.appendChild(script);
}

const AppElementBase = HelpBubbleMixin(PolymerElement) as
    {new (): PolymerElement & HelpBubbleMixinInterface};

export interface AppElement {
  $: {
    customizeDialogIf: DomIf,
    oneGoogleBarClipPath: HTMLElement,
    logo: LogoElement,
  };
}

export class AppElement extends AppElementBase {
  static get is() {
    return 'ntp-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      oneGoogleBarIframeOrigin_: {
        type: String,
        value: OGB_IFRAME_ORIGIN,
      },

      oneGoogleBarIframePath_: {
        type: String,
        value: () => {
          const params = new URLSearchParams();
          params.set(
              'paramsencoded',
              btoa(window.location.search.replace(/^[?]/, '&')));
          return `${OGB_IFRAME_ORIGIN}/one-google-bar?${params}`;
        },
      },

      theme_: {
        observer: 'onThemeChange_',
        type: Object,
      },

      showCustomize_: {
        type: Boolean,
        value: () =>
            WindowProxy.getInstance().url.searchParams.has(CUSTOMIZE_URL_PARAM),
      },

      showCustomizeChromeText_: {
        type: Boolean,
        computed:
            `computeShowCustomizeChromeText_(wallpaperSearchButtonEnabled_,
            showBackgroundImage_)`,
      },

      showWallpaperSearch_: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      selectedCustomizeDialogPage_: {
        type: String,
        value: () =>
            WindowProxy.getInstance().url.searchParams.get(CUSTOMIZE_URL_PARAM),
      },

      showVoiceSearchOverlay_: Boolean,

      showBackgroundImage_: {
        computed: 'computeShowBackgroundImage_(theme_)',
        observer: 'onShowBackgroundImageChange_',
        reflectToAttribute: true,
        type: Boolean,
      },

      backgroundImageAttribution1_: {
        type: String,
        computed: `computeBackgroundImageAttribution1_(theme_)`,
      },

      backgroundImageAttribution2_: {
        type: String,
        computed: `computeBackgroundImageAttribution2_(theme_)`,
      },

      backgroundImageAttributionUrl_: {
        type: String,
        computed: `computeBackgroundImageAttributionUrl_(theme_)`,
      },

      backgroundColor_: {
        computed: 'computeBackgroundColor_(showBackgroundImage_, theme_)',
        type: Object,
      },

      // Used in cr-realbox component via host-context.
      colorSourceIsBaseline: {
        type: Boolean,
        computed: 'computeColorSourceIsBaseline(theme_)',
      },

      logoColor_: {
        type: String,
        computed: 'computeLogoColor_(theme_)',
      },

      singleColoredLogo_: {
        computed: 'computeSingleColoredLogo_(theme_)',
        type: Boolean,
      },

      /**
       * Whether the secondary side can be shown based on the feature state and
       * the width available to the dropdown for the ntp searchbox.
       */
      realboxCanShowSecondarySide: {
        type: Boolean,
        value: () => realboxCanShowSecondarySideMediaQueryList.matches,
        reflectToAttribute: true,
      },

      /**
       * Whether the searchbox secondary side was at any point available to
       * be shown.
       */
      realboxHadSecondarySide: {
        type: Boolean,
        reflectToAttribute: true,
        notify: true,
      },

      realboxIsTall_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('realboxIsTall'),
        reflectToAttribute: true,
      },

      realboxShown_: {
        type: Boolean,
        computed: 'computeRealboxShown_(theme_, showLensUploadDialog_)',
      },

      /* Searchbox width behavior. */
      realboxWidthBehavior_: {
        type: String,
        value: () => loadTimeData.getString('realboxWidthBehavior'),
        reflectToAttribute: true,
      },

      logoEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('logoEnabled'),
      },

      oneGoogleBarEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('oneGoogleBarEnabled'),
      },

      shortcutsEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('shortcutsEnabled'),
      },

      singleRowShortcutsEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('singleRowShortcutsEnabled'),
      },

      modulesFreShown: {
        type: Boolean,
        reflectToAttribute: true,
      },

      middleSlotPromoEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('middleSlotPromoEnabled'),
      },

      modulesEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('modulesEnabled'),
      },

      modulesRedesignedEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('modulesRedesignedEnabled'),
        reflectToAttribute: true,
      },

      mostVisitedReflowOnOverflowEnabled_: {
        type: Boolean,
        value: () =>
            loadTimeData.getBoolean('mostVisitedReflowOnOverflowEnabled'),
        reflectToAttribute: true,
      },

      wideModulesEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('wideModulesEnabled'),
        reflectToAttribute: true,
      },

      middleSlotPromoLoaded_: {
        type: Boolean,
        value: false,
      },

      modulesLoaded_: {
        type: Boolean,
        value: false,
      },

      modulesShownToUser: {
        type: Boolean,
        reflectToAttribute: true,
      },

      /**
       * In order to avoid flicker, the promo and modules are hidden until both
       * are loaded. If modules are disabled, the promo is shown as soon as it
       * is loaded.
       */
      promoAndModulesLoaded_: {
        type: Boolean,
        computed: `computePromoAndModulesLoaded_(middleSlotPromoLoaded_,
            modulesLoaded_)`,
        observer: 'onPromoAndModulesLoadedChange_',
      },

      showLensUploadDialog_: Boolean,

      /**
       * If true, renders additional elements that were not deemed crucial to
       * to show up immediately on load.
       */
      lazyRender_: Boolean,

      scrolledToTop_: {
        type: Boolean,
        value: document.documentElement.scrollTop <= 0,
      },

      wallpaperSearchButtonAnimationEnabled_: {
        type: Boolean,
        value: () =>
            loadTimeData.getBoolean('wallpaperSearchButtonAnimationEnabled'),
        reflectToAttribute: true,
      },

      wallpaperSearchButtonEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('wallpaperSearchButtonEnabled'),
        reflectToAttribute: true,
      },
    };
  }

  static get observers() {
    return [
      'updateOneGoogleBarAppearance_(oneGoogleBarLoaded_, theme_)',
    ];
  }

  private oneGoogleBarIframePath_: string;
  private oneGoogleBarLoaded_: boolean;
  private theme_: Theme;
  private showCustomize_: boolean;
  private showCustomizeChromeText_: boolean;
  private showWallpaperSearch_: boolean;
  private selectedCustomizeDialogPage_: string|null;
  private showVoiceSearchOverlay_: boolean;
  private showBackgroundImage_: boolean;
  private backgroundImageAttribution1_: string;
  private backgroundImageAttribution2_: string;
  private backgroundImageAttributionUrl_: string;
  private backgroundColor_: SkColor;
  private logoColor_: string;
  private singleColoredLogo_: boolean;
  realboxCanShowSecondarySide: boolean;
  realboxHadSecondarySide: boolean;
  private realboxShown_: boolean;
  private showLensUploadDialog_: boolean = false;
  private logoEnabled_: boolean;
  private oneGoogleBarEnabled_: boolean;
  private shortcutsEnabled_: boolean;
  private modulesFreShown: boolean;
  private middleSlotPromoEnabled_: boolean;
  private modulesEnabled_: boolean;
  private modulesRedesignedEnabled_: boolean;
  private middleSlotPromoLoaded_: boolean;
  private modulesLoaded_: boolean;
  private modulesShownToUser: boolean;
  private promoAndModulesLoaded_: boolean;
  private lazyRender_: boolean;
  private scrolledToTop_: boolean;
  private wallpaperSearchButtonAnimationEnabled_: boolean;
  private wallpaperSearchButtonEnabled_: boolean;

  private callbackRouter_: PageCallbackRouter;
  private pageHandler_: PageHandlerRemote;
  private backgroundManager_: BackgroundManager;
  private setThemeListenerId_: number|null = null;
  private setCustomizeChromeSidePanelVisibilityListener_: number|null = null;
  private setWallpaperSearchButtonVisibilityListener_: number|null = null;
  private eventTracker_: EventTracker = new EventTracker();
  private shouldPrintPerformance_: boolean;
  private backgroundImageLoadStartEpoch_: number;
  private backgroundImageLoadStart_: number = 0;
  private showWebstoreToastListenerId_: number|null = null;

  constructor() {
    performance.mark('app-creation-start');
    super();
    this.callbackRouter_ = NewTabPageProxy.getInstance().callbackRouter;
    this.pageHandler_ = NewTabPageProxy.getInstance().handler;
    this.backgroundManager_ = BackgroundManager.getInstance();
    this.shouldPrintPerformance_ =
        new URLSearchParams(location.search).has('print_perf');

    /**
     * Initialized with the start of the performance timeline in case the
     * background image load is not triggered by JS.
     */
    this.backgroundImageLoadStartEpoch_ = performance.timeOrigin;

    chrome.metricsPrivate.recordValue(
        {
          metricName: 'NewTabPage.Height',
          type: chrome.metricsPrivate.MetricTypeType.HISTOGRAM_LINEAR,
          min: 1,
          max: 1000,
          buckets: 200,
        },
        Math.floor(window.innerHeight));
    chrome.metricsPrivate.recordValue(
        {
          metricName: 'NewTabPage.Width',
          type: chrome.metricsPrivate.MetricTypeType.HISTOGRAM_LINEAR,
          min: 1,
          max: 1920,
          buckets: 384,
        },
        Math.floor(window.innerWidth));

    ColorChangeUpdater.forDocument().start();
  }

  override connectedCallback() {
    super.connectedCallback();
    realboxCanShowSecondarySideMediaQueryList.addEventListener(
        'change', this.onRealboxCanShowSecondarySideChanged_.bind(this));
    this.setThemeListenerId_ =
        this.callbackRouter_.setTheme.addListener((theme: Theme) => {
          if (!this.theme_) {
            this.onThemeLoaded_(theme);
          }
          performance.measure('theme-set');
          this.theme_ = theme;
        });
    this.setCustomizeChromeSidePanelVisibilityListener_ =
        this.callbackRouter_.setCustomizeChromeSidePanelVisibility.addListener(
            (visible: boolean) => {
              this.showCustomize_ = visible;
              if (!visible) {
                this.showWallpaperSearch_ = false;
              }
            });
    this.showWebstoreToastListenerId_ =
        this.callbackRouter_.showWebstoreToast.addListener(() => {
          if (this.showCustomize_) {
            const toast = $$<CrToastElement>(this, '#webstoreToast');
            if (toast) {
              toast!.hidden = false;
              toast!.show();
            }
          }
        });
    this.setWallpaperSearchButtonVisibilityListener_ =
        this.callbackRouter_.setWallpaperSearchButtonVisibility.addListener(
            (visible: boolean) => {
              // We only show the button if wallpaper search is enabled when the
              // NTP loads. This prevents the button from showing if Customize
              // Chrome doesn't have the wallpaper search element yet.
              if (!visible) {
                this.wallpaperSearchButtonEnabled_ = visible;
              }
            });

    // Open Customize Chrome if there are Customize Chrome URL params.
    if (this.showCustomize_) {
      this.setCustomizeChromeSidePanelVisible_(this.showCustomize_);
      recordCustomizeChromeOpen(NtpCustomizeChromeEntryPoint.URL);
    }
    this.eventTracker_.add(window, 'message', (event: MessageEvent) => {
      const data = event.data;
      // Something in OneGoogleBar is sending a message that is received here.
      // Need to ignore it.
      if (typeof data !== 'object') {
        return;
      }
      if ('frameType' in data && data.frameType === 'one-google-bar') {
        this.handleOneGoogleBarMessage_(event);
      }
    });
    this.eventTracker_.add(window, 'keydown', this.onWindowKeydown_.bind(this));
    this.eventTracker_.add(
        window, 'click', this.onWindowClick_.bind(this), /*capture=*/ true);
    this.eventTracker_.add(document, 'scroll', () => {
      this.scrolledToTop_ = document.documentElement.scrollTop <= 0;
    });
    if (loadTimeData.getString('backgroundImageUrl')) {
      this.backgroundManager_.getBackgroundImageLoadTime().then(
          time => {
            const duration = time - this.backgroundImageLoadStartEpoch_;
            recordDuration(
                'NewTabPage.Images.ShownTime.BackgroundImage', duration);
            if (this.shouldPrintPerformance_) {
              this.printPerformanceDatum_(
                  'background-image-load', this.backgroundImageLoadStart_,
                  duration);
              this.printPerformanceDatum_(
                  'background-image-loaded',
                  this.backgroundImageLoadStart_ + duration);
            }
          },
          () => {
              // Ignore. Failed to capture background image load time.
          });
    }
    FocusOutlineManager.forDocument(document);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    realboxCanShowSecondarySideMediaQueryList.removeEventListener(
        'change', this.onRealboxCanShowSecondarySideChanged_.bind(this));
    this.callbackRouter_.removeListener(this.setThemeListenerId_!);
    this.callbackRouter_.removeListener(
        this.setCustomizeChromeSidePanelVisibilityListener_!);
    this.callbackRouter_.removeListener(this.showWebstoreToastListenerId_!);
    this.callbackRouter_.removeListener(
        this.setWallpaperSearchButtonVisibilityListener_!);
    this.eventTracker_.removeAll();
  }

  override ready() {
    super.ready();
    this.pageHandler_.onAppRendered(WindowProxy.getInstance().now());
    // Let the browser breathe and then render remaining elements.
    WindowProxy.getInstance().waitForLazyRender().then(() => {
      ensureLazyLoaded();
      this.lazyRender_ = true;
    });
    this.printPerformance_();
    performance.measure('app-creation', 'app-creation-start');
  }

  // Called to update the OGB of relevant NTP state changes.
  private updateOneGoogleBarAppearance_() {
    if (this.oneGoogleBarLoaded_) {
      const isNtpDarkTheme =
          this.theme_ && (!!this.theme_.backgroundImage || this.theme_.isDark);
      $$<IframeElement>(this, '#oneGoogleBar')!.postMessage({
        type: 'updateAppearance',
        // We should be using a light OGB for dark themes and vice versa.
        applyLightTheme: isNtpDarkTheme,
      });
    }
  }

  private computeShowCustomizeChromeText_(): boolean {
    if (this.wallpaperSearchButtonEnabled_) {
      return false;
    }
    return !this.showBackgroundImage_;
  }

  private computeBackgroundImageAttribution1_(): string {
    return this.theme_ && this.theme_.backgroundImageAttribution1 || '';
  }

  private computeBackgroundImageAttribution2_(): string {
    return this.theme_ && this.theme_.backgroundImageAttribution2 || '';
  }

  private computeBackgroundImageAttributionUrl_(): string {
    return this.theme_ && this.theme_.backgroundImageAttributionUrl ?
        this.theme_.backgroundImageAttributionUrl.url :
        '';
  }

  private computeRealboxShown_(theme: Theme, showLensUploadDialog: boolean):
      boolean {
    // Do not show the realbox if the upload dialog is showing.
    return theme && !showLensUploadDialog;
  }

  private computePromoAndModulesLoaded_(): boolean {
    return (!loadTimeData.getBoolean('middleSlotPromoEnabled') ||
            this.middleSlotPromoLoaded_) &&
        (!loadTimeData.getBoolean('modulesEnabled') || this.modulesLoaded_);
  }

  private onRealboxCanShowSecondarySideChanged_(e: MediaQueryListEvent) {
    this.realboxCanShowSecondarySide = e.matches;
  }

  private async onLazyRendered_() {
    // Integration tests use this attribute to determine when lazy load has
    // completed.
    document.documentElement.setAttribute('lazy-loaded', String(true));
    this.registerHelpBubble(
        CUSTOMIZE_CHROME_BUTTON_ELEMENT_ID, '#customizeButton', {fixed: true});
    this.pageHandler_.maybeShowFeaturePromo(IphFeature.kCustomizeChrome);
  }

  private onOpenVoiceSearch_() {
    this.showVoiceSearchOverlay_ = true;
    recordVoiceAction(VoiceAction.ACTIVATE_SEARCH_BOX);
  }

  private onOpenLensSearch_() {
    this.showLensUploadDialog_ = true;
  }

  private onCloseLensSearch_() {
    this.showLensUploadDialog_ = false;
  }

  private onCustomizeClick_() {
    // Let side panel decide what page or section to show.
    this.selectedCustomizeDialogPage_ = null;
    this.setCustomizeChromeSidePanelVisible_(!this.showCustomize_);
    if (!this.showCustomize_) {
      this.pageHandler_.incrementCustomizeChromeButtonOpenCount();
      recordCustomizeChromeOpen(NtpCustomizeChromeEntryPoint.CUSTOMIZE_BUTTON);
    }
  }

  private onWallpaperSearchClick_() {
    // Close the side panel if Wallpaper Search is open.
    if (this.showCustomize_ && this.showWallpaperSearch_) {
      this.selectedCustomizeDialogPage_ = null;
      this.setCustomizeChromeSidePanelVisible_(!this.showCustomize_);
      return;
    }

    // Open Wallpaper Search if the side panel is closed. Otherwise, navigate
    // the side panel to Wallpaper Search.
    this.selectedCustomizeDialogPage_ = CustomizeDialogPage.WALLPAPER_SEARCH;
    this.showWallpaperSearch_ = true;
    this.setCustomizeChromeSidePanelVisible_(this.showWallpaperSearch_);
    if (!this.showCustomize_) {
      this.pageHandler_.incrementCustomizeChromeButtonOpenCount();
      recordCustomizeChromeOpen(
          NtpCustomizeChromeEntryPoint.WALLPAPER_SEARCH_BUTTON);
    }
  }

  private onVoiceSearchOverlayClose_() {
    this.showVoiceSearchOverlay_ = false;
  }

  /**
   * Handles <CTRL> + <SHIFT> + <.> (also <CMD> + <SHIFT> + <.> on mac) to open
   * voice search.
   */
  private onWindowKeydown_(e: KeyboardEvent) {
    let ctrlKeyPressed = e.ctrlKey;
    // <if expr="is_macosx">
    ctrlKeyPressed = ctrlKeyPressed || e.metaKey;
    // </if>
    if (ctrlKeyPressed && e.code === 'Period' && e.shiftKey) {
      this.showVoiceSearchOverlay_ = true;
      recordVoiceAction(VoiceAction.ACTIVATE_KEYBOARD);
    }
  }

  private rgbaOrInherit_(skColor: SkColor|null): string {
    return skColor ? skColorToRgba(skColor) : 'inherit';
  }

  private computeShowBackgroundImage_(): boolean {
    return !!this.theme_ && !!this.theme_.backgroundImage;
  }

  private onShowBackgroundImageChange_() {
    this.backgroundManager_.setShowBackgroundImage(this.showBackgroundImage_);
  }

  private onThemeChange_() {
    if (this.theme_) {
      this.backgroundManager_.setBackgroundColor(this.theme_.backgroundColor);
    }
    this.updateBackgroundImagePath_();
  }


  private onThemeLoaded_(theme: Theme) {
    chrome.metricsPrivate.recordEnumerationValue(
        'NewTabPage.BackgroundImageSource',
        (theme.backgroundImage ? theme.backgroundImage.imageSource :
                                 NtpBackgroundImageSource.kNoImage),
        NtpBackgroundImageSource.MAX_VALUE + 1);

    chrome.metricsPrivate.recordSparseValueWithPersistentHash(
        'NewTabPage.Collections.IdOnLoad',
        theme.backgroundImageCollectionId ?? '');
  }

  private onPromoAndModulesLoadedChange_() {
    if (this.promoAndModulesLoaded_ &&
        loadTimeData.getBoolean('modulesEnabled')) {
      recordLoadDuration(
          'NewTabPage.Modules.ShownTime', WindowProxy.getInstance().now());
    }
  }

  /**
   * Set the #backgroundImage |path| only when different and non-empty. Reset
   * the customize dialog background selection if the dialog is closed.
   *
   * The ntp-untrusted-iframe |path| is set directly. When using a data binding
   * instead, the quick updates to the |path| result in iframe loading an error
   * page.
   */
  private updateBackgroundImagePath_() {
    const backgroundImage = this.theme_ && this.theme_.backgroundImage;

    if (backgroundImage) {
      this.backgroundManager_.setBackgroundImage(backgroundImage);
    }
  }

  private computeBackgroundColor_(): SkColor|null {
    if (this.showBackgroundImage_) {
      return null;
    }
    return this.theme_ && this.theme_.backgroundColor;
  }

  private computeColorSourceIsBaseline(): boolean {
    return this.theme_.isBaseline;
  }

  private computeLogoColor_(): SkColor|null {
    return this.theme_ &&
        (this.theme_.logoColor ||
         (this.theme_.isDark ? hexColorToSkColor('#ffffff') : null));
  }

  private computeSingleColoredLogo_(): boolean {
    return this.theme_ && (!!this.theme_.logoColor || this.theme_.isDark);
  }

  /**
   * Sends the command received from the given source and origin to the browser.
   * Relays the browser response to whether or not a promo containing the given
   * command can be shown back to the source promo frame. |commandSource| and
   * |commandOrigin| are used only to send the response back to the source promo
   * frame and should not be used for anything else.
   * @param  messageData Data received from the source promo frame.
   * @param commandSource Source promo frame.
   * @param commandOrigin Origin of the source promo frame.
   */
  private canShowPromoWithBrowserCommand_(
      messageData: CanShowPromoWithBrowserCommandData, commandSource: Window,
      commandOrigin: string) {
    // Make sure we don't send unsupported commands to the browser.
    /** @type {!Command} */
    const commandId = Object.values(Command).includes(messageData.commandId) ?
        messageData.commandId :
        Command.kUnknownCommand;

    BrowserCommandProxy.getInstance().handler.canExecuteCommand(commandId).then(
        ({canExecute}) => {
          const response = {
            messageType: messageData.messageType,
            [messageData.commandId]: canExecute,
          };
          commandSource.postMessage(response, commandOrigin);
        });
  }

  /**
   * Sends the command and the accompanying mouse click info received from the
   * promo of the given source and origin to the browser. Relays the execution
   * status response back to the source promo frame. |commandSource| and
   * |commandOrigin| are used only to send the execution status response back to
   * the source promo frame and should not be used for anything else.
   * @param commandData Command and mouse click info.
   * @param commandSource Source promo frame.
   * @param commandOrigin Origin of the source promo frame.
   */
  private executePromoBrowserCommand_(
      commandData: ExecutePromoBrowserCommandData, commandSource: Window,
      commandOrigin: string) {
    // Make sure we don't send unsupported commands to the browser.
    const commandId = Object.values(Command).includes(commandData.commandId) ?
        commandData.commandId :
        Command.kUnknownCommand;

    BrowserCommandProxy.getInstance()
        .handler.executeCommand(commandId, commandData.clickInfo)
        .then(({commandExecuted}) => {
          commandSource.postMessage(commandExecuted, commandOrigin);
        });
  }

  /**
   * Handles messages from the OneGoogleBar iframe. The messages that are
   * handled include show bar on load and overlay updates.
   *
   * 'overlaysUpdated' message includes the updated array of overlay rects that
   * are shown.
   */
  private handleOneGoogleBarMessage_(event: MessageEvent) {
    const data = event.data;
    if (data.messageType === 'loaded') {
      const oneGoogleBar = $$<IframeElement>(this, '#oneGoogleBar')!;
      oneGoogleBar.style.clipPath = 'url(#oneGoogleBarClipPath)';
      oneGoogleBar.style.zIndex = '1000';
      this.oneGoogleBarLoaded_ = true;
      this.pageHandler_.onOneGoogleBarRendered(WindowProxy.getInstance().now());
    } else if (data.messageType === 'overlaysUpdated') {
      this.$.oneGoogleBarClipPath.querySelectorAll('rect').forEach(el => {
        el.remove();
      });
      const overlayRects = data.data as DOMRect[];
      overlayRects.forEach(({x, y, width, height}) => {
        const rectElement =
            document.createElementNS('http://www.w3.org/2000/svg', 'rect');
        // Add 8px around every rect to ensure shadows are not cutoff.
        rectElement.setAttribute('x', `${x - 8}`);
        rectElement.setAttribute('y', `${y - 8}`);
        rectElement.setAttribute('width', `${width + 16}`);
        rectElement.setAttribute('height', `${height + 16}`);
        this.$.oneGoogleBarClipPath.appendChild(rectElement);
      });
    } else if (data.messageType === 'can-show-promo-with-browser-command') {
      this.canShowPromoWithBrowserCommand_(
          data, event.source as Window, event.origin);
    } else if (data.messageType === 'execute-browser-command') {
      this.executePromoBrowserCommand_(
          data.data, event.source as Window, event.origin);
    } else if (data.messageType === 'click') {
      recordClick(NtpElement.ONE_GOOGLE_BAR);
    }
  }

  private onMiddleSlotPromoLoaded_() {
    this.middleSlotPromoLoaded_ = true;
  }

  private onModulesLoaded_() {
    this.modulesLoaded_ = true;
  }

  private onCustomizeModule_() {
    this.showCustomize_ = true;
    this.selectedCustomizeDialogPage_ = CustomizeDialogPage.MODULES;
    recordCustomizeChromeOpen(NtpCustomizeChromeEntryPoint.MODULE);
    this.setCustomizeChromeSidePanelVisible_(this.showCustomize_);
  }

  private setCustomizeChromeSidePanelVisible_(visible: boolean) {
    let section: CustomizeChromeSection = CustomizeChromeSection.kUnspecified;
    switch (this.selectedCustomizeDialogPage_) {
      case CustomizeDialogPage.BACKGROUNDS:
      case CustomizeDialogPage.THEMES:
        section = CustomizeChromeSection.kAppearance;
        break;
      case CustomizeDialogPage.SHORTCUTS:
        section = CustomizeChromeSection.kShortcuts;
        break;
      case CustomizeDialogPage.MODULES:
        section = CustomizeChromeSection.kModules;
        break;
      case CustomizeDialogPage.WALLPAPER_SEARCH:
        section = CustomizeChromeSection.kWallpaperSearch;
        break;
    }
    this.pageHandler_.setCustomizeChromeSidePanelVisible(visible, section);
  }

  private printPerformanceDatum_(
      name: string, time: number, auxTime: number = 0) {
    if (!this.shouldPrintPerformance_) {
      return;
    }

    console.info(
        !auxTime ? `${name}: ${time}` : `${name}: ${time} (${auxTime})`);
  }

  /**
   * Prints performance measurements to the console. Also, installs  performance
   * observer to continuously print performance measurements after.
   */
  private printPerformance_() {
    if (!this.shouldPrintPerformance_) {
      return;
    }
    const entryTypes = ['paint', 'measure'];
    const log = (entry: PerformanceEntry) => {
      this.printPerformanceDatum_(
          entry.name, entry.duration ? entry.duration : entry.startTime,
          entry.duration && entry.startTime ? entry.startTime : 0);
    };
    const observer = new PerformanceObserver(list => {
      list.getEntries().forEach((entry) => {
        log(entry);
      });
    });
    observer.observe({entryTypes: entryTypes});
    performance.getEntries().forEach((entry) => {
      if (!entryTypes.includes(entry.entryType)) {
        return;
      }
      log(entry);
    });
  }

  private onWebstoreToastButtonClick_() {
    window.location.assign(
        `https://chrome.google.com/webstore/category/collection/chrome_color_themes?hl=${
            window.navigator.language}`);
  }

  private onWindowClick_(e: Event) {
    if (e.composedPath() && e.composedPath()[0] === $$(this, '#content')) {
      recordClick(NtpElement.BACKGROUND);
      return;
    }
    for (const target of e.composedPath()) {
      switch (target) {
        case $$(this, 'ntp-logo'):
          recordClick(NtpElement.LOGO);
          return;
        case $$(this, 'cr-realbox'):
          recordClick(NtpElement.REALBOX);
          return;
        case $$(this, 'cr-most-visited'):
          recordClick(NtpElement.MOST_VISITED);
          return;
        case $$(this, 'ntp-middle-slot-promo'):
          recordClick(NtpElement.MIDDLE_SLOT_PROMO);
          return;
        case $$(this, '#modules'):
          recordClick(NtpElement.MODULE);
          return;
        case $$(this, '#customizeButton'):
          recordClick(NtpElement.CUSTOMIZE_BUTTON);
          return;
        case $$(this, 'ntp-customize-dialog'):
          recordClick(NtpElement.CUSTOMIZE_DIALOG);
          return;
        case $$(this, '#wallpaperSearchButton'):
          recordClick(NtpElement.WALLPAPER_SEARCH_BUTTON);
          return;
      }
    }
    recordClick(NtpElement.OTHER);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-app': AppElement;
  }
}

customElements.define(AppElement.is, AppElement);
