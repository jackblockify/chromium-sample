// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.readingMode API */

// Add non-standard function to element for TS to compile correctly.
interface Element {
  scrollIntoViewIfNeeded: () => void;
}

declare namespace chrome {
  export namespace readingMode {
    /////////////////////////////////////////////////////////////////////
    // Implemented in read_anything_app_controller.cc and consumed by ts.
    /////////////////////////////////////////////////////////////////////

    // The root AXNodeID of the tree to be displayed.
    let rootId: number;

    // Selection information. The selection must be a forward selection, with
    // the start node and offset before the end node and offset. Through
    // experimentation, it was observed that programmatically created backwards
    // ranges are collapsed to the start node. A test was added in
    // read_anything_app_test to confirm this assumption.
    let startNodeId: number;
    let startOffset: number;
    let endNodeId: number;
    let endOffset: number;

    // Items in the ReadAnythingTheme struct, see read_anything.mojom for info.
    let fontName: string;
    let fontSize: number;
    let linksEnabled: boolean;
    let imagesEnabled: boolean;
    let imagesFeatureEnabled: boolean;
    let foregroundColor: number;
    let backgroundColor: number;
    let lineSpacing: number;
    let letterSpacing: number;

    // The current color theme value.
    let colorTheme: number;

    // Current audio settings values.
    let speechRate: number;
    let highlightGranularity: number;

    // Enum values for various visual theme changes.
    let standardLineSpacing: number;
    let looseLineSpacing: number;
    let veryLooseLineSpacing: number;
    let standardLetterSpacing: number;
    let wideLetterSpacing: number;
    let veryWideLetterSpacing: number;
    let defaultTheme: number;
    let lightTheme: number;
    let darkTheme: number;
    let yellowTheme: number;
    let blueTheme: number;
    let highlightOn: number;

    // Whether the WebUI toolbar feature flag is enabled.
    let isWebUIToolbarVisible: boolean;

    // Whether the Read Aloud feature flag is enabled.
    let isReadAloudEnabled: boolean;

    // Whether the automatic voice switching feature flag is enabled.
    let isAutoVoiceSwitchingEnabled: boolean;

    // Whether the language pack download feature flag is enabled.
    let isLanguagePackDownloadingEnabled: boolean;

    let isAutomaticWordHighlightingEnabled: boolean;

    // Indicates if this page is a Google doc.
    let isGoogleDocs: boolean;

    // Fonts supported by the browser's preferred language.
    let supportedFonts: string[];

    // The base language code that should be used for speech synthesis voices.
    let baseLanguageForSpeech: string;

    // The fallback language, corresponding to the browser language, that
    // should only be used when baseLanguageForSpeech is unavailable.
    let defaultLanguageForSpeech: string;

    // If the current platform is ChromeOS Ash.
    let isChromeOsAsh: boolean;

    // If distillations have been queued up.
    let requiresDistillation: boolean;

    // Returns the stored user voice preference for the current language
    function getStoredVoice(): string;

    // Returns the stored user preference for enabled languages.
    function getLanguagesEnabledInPref(): string[];

    // Returns a list of AXNodeIDs corresponding to the unignored children of
    // the AXNode for the provided AXNodeID. If there is a selection contained
    // in this node, only returns children which are partially or entirely
    // contained within the selection.
    function getChildren(nodeId: number): number[];

    // Returns content of "data-font-css" html attribute. This is needed for
    // rendering content from annotated canvas in Google Docs.
    function getDataFontCss(nodeId: number): string;

    // Returns the HTML tag of the AXNode for the provided AXNodeID.
    function getHtmlTag(nodeId: number): string;

    // Returns the language of the AXNode for the provided AXNodeID.
    function getLanguage(nodeId: number): string;

    // Returns the text content of the AXNode for the provided AXNodeID. If a
    // selection begins or ends in this node, truncates the text to only return
    // the selected text.
    function getTextContent(nodeId: number): string;

    // Returns the text direction of the AXNode for the provided AXNodeID.
    function getTextDirection(nodeId: number): string;

    // Returns the url of the AXNode for the provided AXNodeID.
    function getUrl(nodeId: number): string;

    // Returns the alt text of the AXNode for the provided AXNodeID.
    function getAltText(nodeId: number): string;

    // Returns true if the text node / element should be bolded.
    function shouldBold(nodeId: number): boolean;

    // Returns true if the element has overline text styling.
    function isOverline(nodeId: number): boolean;

    // Returns true if the element is a leaf node.
    function isLeafNode(nodeId: number): boolean;

    // Connects to the browser process. Called by ts when the read anything
    // element is added to the document.
    function onConnected(): void;

    // Called when a user tries to copy text from reading mode with keyboard
    // shortcuts.
    function onCopy(): void;

    // Called when speech is paused or played.
    function onSpeechPlayingStateChanged(paused: boolean): void;

    // Called when the Read Anything panel is scrolled.
    function onScroll(onSelection: boolean): void;

    // Called when a user clicks a link. NodeID is an AXNodeID which identifies
    // the link's corresponding AXNode in the main pane.
    function onLinkClicked(nodeId: number): void;

    // Called when the line spacing is changed via the webui toolbar.
    function onStandardLineSpacing(): void;
    function onLooseLineSpacing(): void;
    function onVeryLooseLineSpacing(): void;

    // Called when a user makes a font size change via the webui toolbar.
    function onFontSizeChanged(increase: boolean): void;
    function onFontSizeReset(): void;

    // Called when a user toggles links via the webui toolbar.
    function onLinksEnabledToggled(): void;

    // Called when a user toggles images via the webui toolbar.
    function onImagesEnabledToggled(): void;

    // Called when the letter spacing is changed via the webui toolbar.
    function onStandardLetterSpacing(): void;
    function onWideLetterSpacing(): void;
    function onVeryWideLetterSpacing(): void;

    // Called when the color theme is changed via the webui toolbar.
    function onDefaultTheme(): void;
    function onLightTheme(): void;
    function onDarkTheme(): void;
    function onYellowTheme(): void;
    function onBlueTheme(): void;

    // Called when the font is changed via the webui toolbar.
    function onFontChange(font: string): void;

    // Called when the speech rate is changed via the webui toolbar.
    function onSpeechRateChange(rate: number): void;

    // Called when the voice used for speech is changed via the webui toolbar.
    function onVoiceChange(voice: string, lang: string): void;

    // Called when a language is enabled/disabled for Read Aloud
    // via the webui language menu.
    function onLanguagePrefChange(lang: string, enabled: boolean): void;

    // Called when the highlight granularity is changed via the webui toolbar.
    function turnedHighlightOn(): void;
    function turnedHighlightOff(): void;

    // Returns the actual spacing value to use based on the given lineSpacing
    // category.
    function getLineSpacingValue(lineSpacing: number): number;

    // Returns the actual spacing value to use based on the given letterSpacing
    // category.
    function getLetterSpacingValue(letterSpacing: number): number;

    // Called when a user makes a selection change. AnchorNodeID and
    // focusAXNodeID are AXNodeIDs which identify the anchor and focus AXNodes
    // in the main pane. The selection can either be forward or backwards.
    function onSelectionChange(
        anchorNodeId: number, anchorOffset: number, focusNodeId: number,
        focusOffset: number): void;

    // Called when a user collapses the selection. This is usually accomplished
    // by clicking.
    function onCollapseSelection(): void;

    // Called when we are restarting read aloud after we've already started
    // playing speech.
    function onRestartReadAloud(): void;

    // Set the content. Used by tests only.
    // SnapshotLite is a data structure which resembles an AXTreeUpdate. E.g.:
    //   const axTree = {
    //     rootId: 1,
    //     nodes: [
    //       {
    //         id: 1,
    //         role: 'rootWebArea',
    //         childIds: [2],
    //       },
    //       {
    //         id: 2,
    //         role: 'staticText',
    //         name: 'Some text.',
    //       },
    //     ],
    //   };
    function setContentForTesting(
        snapshotLite: Object, contentNodeIds: number[]): void;

    // Set the theme. Used by tests only.
    function setThemeForTesting(
        fontName: string, fontSize: number, linksEnabled: boolean,
        foregroundColor: number, backgroundColor: number, lineSpacing: number,
        letterSpacing: number): void;

    // Sets the page language. Used by tests only.
    function setLanguageForTesting(code: string): void;

    // Called when the side panel has finished loading and it's safe to call
    // SidePanelWebUIView::ShowUI
    function shouldShowUi(): boolean;

    ////////////////////////////////////////////////////////////////
    // Implemented in read_anything/app.ts and called by native c++.
    ////////////////////////////////////////////////////////////////

    // Display a loading screen to tell the user we are distilling the page.
    function showLoading(): void;

    // Display the empty state page to tell the user we can't distill the page.
    function showEmpty(): void;

    // Ping that an AXTree has been distilled for the active tab's render frame
    // and is available to consume.
    function updateContent(): void;

    // Redraws links when the enabled state changes.
    function updateLinks(): void;

    // Redraws images when the enabled state changes.
    function updateImages(): void;

    // Updates an images src attribute with a data url. The data url must have
    // been requested first.
    function updateImage(nodeId: number): void;

    // Ping that the selection has been updated.
    function updateSelection(): void;

    // Ping that the theme choices of the user have been changed using the
    // toolbar and are ready to consume.
    function updateTheme(): void;

    // Read Aloud state should be updated if the lock screen state changes.
    function onLockScreen(): void;

    // Called with the response of sendGetVoicePackInfoRequest()
    function updateVoicePackStatus(lang: string, status: string): void;

    // Called with the response of sendInstallVoicePackRequest()
    function updateVoicePackStatusFromInstallResponse(
        lang: string, status: string): void;

    // Ping that the theme choices of the user have been retrieved from
    // preferences and can be used to set up the page.
    function restoreSettingsFromPrefs(): void;

    // Inits the AXPosition instance in ReadAnythingAppController with the
    // starting node. Currently needed to orient the AXPosition to the correct
    // position, but we should be able to remove this in the future.
    function initAxPositionWithNode(startingNodeId: number): void;

    // Gets the starting text index for the current Read Aloud text segment
    // for the given node. nodeId should be a node returned by getCurrentText.
    // Returns -1 if the node is invalid.
    function getCurrentTextStartIndex(nodeId: number): number;

    // Gets the ending text index for the current Read Aloud text segment
    // for the given node. nodeId should be a node returned by getCurrentText or
    // getPreviousText. Returns -1 if the node is invalid.
    function getCurrentTextEndIndex(nodeId: number): number;

    // Gets the nodes of the  next text that should be spoken and highlighted.
    // Use getCurrentTextStartIndex and getCurrentTextEndIndex to get the bounds
    // for text associated with these nodes.
    function getCurrentText(): number[];

    // Increments the processed_granularity_index_ in ReadAnythingAppModel,
    // effectively updating ReadAloud's state of the current granularity to
    // refer to the next granularity.
    function movePositionToNextGranularity(): void;

    // Decrements the processed_granularity_index_ in ReadAnythingAppModel,
    // effectively updating ReadAloud's state of the current granularity to
    // refer to the previous granularity.
    function movePositionToPreviousGranularity(): void;

    // Signal that the page language has changed.
    function languageChanged(): void;

    // Gets the accessible text boundary for the given string
    function getAccessibleBoundary(text: string, maxSpeechLength: number):
        number;

    // Requests the image in the form of a data url. The result will then be
    // stored in the AXNode which can be fetched on content update.
    function requestImageDataUrl(nodeId: number): void;

    // Gets the stored image data url from the AXNode.
    function getImageDataUrl(nodeId: number): string;

    // Gets the readable name for a locale code
    function getDisplayNameForLocale(locale: string, displayLocale: string):
        string;

    // Sends an async request to get the status of a Natural voice pack for a
    // specific language. The response is sent back to the UI via
    // updateVoicePackStatus()
    function sendGetVoicePackInfoRequest(language: string): void;

    // Sends an async request to install a  Natural voice pack for a
    // specific language. The response is sent back to the UI via
    // updateVoicePackStatusFromInstallResponse()
    function sendInstallVoicePackRequest(language: string): void;

    // Log UmaHistogram
    function logMetric(time: number, metricName: string): void;

    // Log UmaHistogramLong
    function logLongMetric(time: number, metricName: string): void;

    // Log UmaHistogramCount
    function incrementMetricCount(metricName: string): void;

    // Log speech errors.
    function logSpeechError(errorCode: string): void;

    // Returns the node id associated with the index within the given text
    // segment.
    // For example, for a segment of text composed of two nodes:
    // Node 1: "Hello, this is a "
    // Node 2: "segment of text."
    // An index of "20" will return the node id associated with node 2.
    function getNodeIdForCurrentSegmentIndex(index: number): number;

    // The highlight length of the next word starting at the given index within
    // the current segment.
    function getNextWordHighlightLength(index: number): number;
  }
}
