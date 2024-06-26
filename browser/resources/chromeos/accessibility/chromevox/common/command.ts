// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** List of commands. Please keep list alphabetical. */
export enum Command {
  ANNOUNCE_BATTERY_DESCRIPTION = 'announceBatteryDescription',
  ANNOUNCE_HEADERS = 'announceHeaders',
  ANNOUNCE_RICH_TEXT_DESCRIPTION = 'announceRichTextDescription',
  AUTORUNNER = 'autorunner',
  BACKWARD = 'backward',
  BOTTOM = 'bottom',
  CONTEXT_MENU = 'contextMenu',
  COPY = 'copy',
  CYCLE_PUNCTUATION_ECHO = 'cyclePunctuationEcho',
  CYCLE_TYPING_ECHO = 'cycleTypingEcho',
  DEBUG = 'debug',
  DECREASE_TTS_PITCH = 'decreaseTtsPitch',
  DECREASE_TTS_RATE = 'decreaseTtsRate',
  DECREASE_TTS_VOLUME = 'decreaseTtsVolume',
  DISABLE_CHROMEVOX_ARC_SUPPORT_FOR_CURRENT_APP =
      'disableChromeVoxArcSupportForCurrentApp',
  DISABLE_LOGGING = 'disableLogging',
  DUMP_TREE = 'dumpTree',
  ENABLE_CHROMEVOX_ARC_SUPPORT_FOR_CURRENT_APP =
      'enableChromeVoxArcSupportForCurrentApp',
  ENABLE_CONSOLE_TTS = 'enableConsoleTts',
  ENABLE_LOGGING = 'enableLogging',
  ENTER_SHIFTER = 'enterShifter',
  EXIT_SHIFTER = 'exitShifter',
  EXIT_SHIFTER_CONTENT = 'exitShifterContent',
  FORCE_CLICK_ON_CURRENT_ITEM = 'forceClickOnCurrentItem',
  FORCE_DOUBLE_CLICK_ON_CURRENT_ITEM = 'forceDoubleClickOnCurrentItem',
  FORCE_LONG_CLICK_ON_CURRENT_ITEM = 'forceLongClickOnCurrentItem',
  FORWARD = 'forward',
  FULLY_DESCRIBE = 'fullyDescribe',
  GO_TO_COL_FIRST_CELL = 'goToColFirstCell',
  GO_TO_COL_LAST_CELL = 'goToColLastCell',
  GO_TO_FIRST_CELL = 'goToFirstCell',
  GO_TO_LAST_CELL = 'goToLastCell',
  GO_TO_ROW_FIRST_CELL = 'goToRowFirstCell',
  GO_TO_ROW_LAST_CELL = 'goToRowLastCell',
  HANDLE_TAB = 'handleTab',
  HANDLE_TAB_PREV = 'handleTabPrev',
  HELP = 'help',
  INCREASE_TTS_PITCH = 'increaseTtsPitch',
  INCREASE_TTS_RATE = 'increaseTtsRate',
  INCREASE_TTS_VOLUME = 'increaseTtsVolume',
  JUMP_TO_BOTTOM = 'jumpToBottom',
  JUMP_TO_DETAILS = 'jumpToDetails',
  JUMP_TO_TOP = 'jumpToTop',
  LEFT = 'left',
  LINE_DOWN = 'lineDown',
  LINE_UP = 'lineUp',
  MOVE_TO_END_OF_LINE = 'moveToEndOfLine',
  MOVE_TO_START_OF_LINE = 'moveToStartOfLine',
  NOP = 'nop',
  NATIVE_NEXT_CHARACTER = 'nativeNextCharacter',
  NATIVE_NEXT_WORD = 'nativeNextWord',
  NATIVE_PREVIOUS_CHARACTER = 'nativePreviousCharacter',
  NATIVE_PREVIOUS_WORD = 'nativePreviousWord',
  NEXT_ARTICLE = 'nextArticle',
  NEXT_AT_GRANULARITY = 'nextAtGranularity',
  NEXT_BUTTON = 'nextButton',
  NEXT_CHARACTER = 'nextCharacter',
  NEXT_CHECKBOX = 'nextCheckbox',
  NEXT_COL = 'nextCol',
  NEXT_COMBO_BOX = 'nextComboBox',
  NEXT_CONTROL = 'nextControl',
  NEXT_EDIT_TEXT = 'nextEditText',
  NEXT_FORM_FIELD = 'nextFormField',
  NEXT_GRANULARITY = 'nextGranularity',
  NEXT_GRAPHIC = 'nextGraphic',
  NEXT_GROUP = 'nextGroup',
  NEXT_HEADING = 'nextHeading',
  NEXT_HEADING_1 = 'nextHeading1',
  NEXT_HEADING_2 = 'nextHeading2',
  NEXT_HEADING_3 = 'nextHeading3',
  NEXT_HEADING_4 = 'nextHeading4',
  NEXT_HEADING_5 = 'nextHeading5',
  NEXT_HEADING_6 = 'nextHeading6',
  NEXT_INVALID_ITEM = 'nextInvalidItem',
  NEXT_LANDMARK = 'nextLandmark',
  NEXT_LINE = 'nextLine',
  NEXT_LINK = 'nextLink',
  NEXT_LIST = 'nextList',
  NEXT_LIST_ITEM = 'nextListItem',
  NEXT_MATH = 'nextMath',
  NEXT_MEDIA = 'nextMedia',
  NEXT_OBJECT = 'nextObject',
  NEXT_PAGE = 'nextPage',
  NEXT_RADIO = 'nextRadio',
  NEXT_ROW = 'nextRow',
  NEXT_SECTION = 'nextSection',
  NEXT_SENTENCE = 'nextSentence',
  NEXT_SIMILAR_ITEM = 'nextSimilarItem',
  NEXT_SLIDER = 'nextSlider',
  NEXT_TABLE = 'nextTable',
  NEXT_VISITED_LINK = 'nextVisitedLink',
  NEXT_WORD = 'nextWord',
  OPEN_CHROMEVOX_MENUS = 'openChromeVoxMenus',
  OPEN_KEYBOARD_SHORTCUTS = 'openKeyboardShortcuts',
  OPEN_LONG_DESC = 'openLongDesc',
  PAN_LEFT = 'panLeft',
  PAN_RIGHT = 'panRight',
  PASS_THROUGH_MODE = 'passThroughMode',
  PAUSE_ALL_MEDIA = 'pauseAllMedia',
  PREVIOUS_ARTICLE = 'previousArticle',
  PREVIOUS_AT_GRANULARITY = 'previousAtGranularity',
  PREVIOUS_BUTTON = 'previousButton',
  PREVIOUS_CHARACTER = 'previousCharacter',
  PREVIOUS_CHECKBOX = 'previousCheckbox',
  PREVIOUS_COMBO_BOX = 'previousComboBox',
  PREVIOUS_COL = 'previousCol',
  PREVIOUS_CONTROL = 'previousControl',
  PREVIOUS_EDIT_TEXT = 'previousEditText',
  PREVIOUS_FORM_FIELD = 'previousFormField',
  PREVIOUS_GRANULARITY = 'previousGranularity',
  PREVIOUS_GRAPHIC = 'previousGraphic',
  PREVIOUS_GROUP = 'previousGroup',
  PREVIOUS_HEADING = 'previousHeading',
  PREVIOUS_HEADING_1 = 'previousHeading1',
  PREVIOUS_HEADING_2 = 'previousHeading2',
  PREVIOUS_HEADING_3 = 'previousHeading3',
  PREVIOUS_HEADING_4 = 'previousHeading4',
  PREVIOUS_HEADING_5 = 'previousHeading5',
  PREVIOUS_HEADING_6 = 'previousHeading6',
  PREVIOUS_INVALID_ITEM = 'previousInvalidItem',
  PREVIOUS_LANDMARK = 'previousLandmark',
  PREVIOUS_LINE = 'previousLine',
  PREVIOUS_LINK = 'previousLink',
  PREVIOUS_LIST = 'previousList',
  PREVIOUS_LIST_ITEM = 'previousListItem',
  PREVIOUS_MATH = 'previousMath',
  PREVIOUS_MEDIA = 'previousMedia',
  PREVIOUS_OBJECT = 'previousObject',
  PREVIOUS_PAGE = 'previousPage',
  PREVIOUS_RADIO = 'previousRadio',
  PREVIOUS_ROW = 'previousRow',
  PREVIOUS_SECTION = 'previousSection',
  PREVIOUS_SENTENCE = 'previousSentence',
  PREVIOUS_SIMILAR_ITEM = 'previousSimilarItem',
  PREVIOUS_SLIDER = 'previousSlider',
  PREVIOUS_TABLE = 'previousTable',
  PREVIOUS_VISITED_LINK = 'previousVisitedLink',
  PREVIOUS_WORD = 'previousWord',
  READ_CURRENT_TITLE = 'readCurrentTitle',
  READ_CURRENT_URL = 'readCurrentURL',
  READ_FROM_HERE = 'readFromHere',
  READ_LINK_URL = 'readLinkURL',
  READ_PHONETIC_PRONUNCIATION = 'readPhoneticPronunciation',
  REPORT_ISSUE = 'reportIssue',
  RESET_TEXT_TO_SPEECH_SETTINGS = 'resetTextToSpeechSettings',
  RIGHT = 'right',
  ROUTING = 'routing',
  SCROLL_BACKWARD = 'scrollBackward',
  SCROLL_FORWARD = 'scrollForward',
  SHOW_ACTIONS_MENU = 'showActionsMenu',
  SHOW_FORMS_LIST = 'showFormsList',
  SHOW_HEADINGS_LIST = 'showHeadingsList',
  SHOW_LANDMARKS_LIST = 'showLandmarksList',
  SHOW_LEARN_MODE_PAGE = 'showLearnModePage',
  SHOW_LINKS_LIST = 'showLinksList',
  SHOW_LOG_PAGE = 'showLogPage',
  SHOW_OPTIONS_PAGE = 'showOptionsPage',
  SHOW_PANEL_MENU_MOST_RECENT = 'showPanelMenuMostRecent',
  SHOW_TABLES_LIST = 'showTablesList',
  SHOW_TALKBACK_KEYBOARD_SHORTCUTS = 'showTalkBackKeyboardShortcuts',
  SHOW_TTS_SETTINGS = 'showTtsSettings',
  SPEAK_TABLE_LOCATION = 'speakTableLocation',
  SPEAK_TIME_AND_DATE = 'speakTimeAndDate',
  START_HISTORY_RECORDING = 'startHistoryRecording',
  STOP_HISTORY_RECORDING = 'stopHistoryRecording',
  STOP_SPEECH = 'stopSpeech',
  TOGGLE_BRAILLE_CAPTIONS = 'toggleBrailleCaptions',
  TOGGLE_BRAILLE_TABLE = 'toggleBrailleTable',
  TOGGLE_DICTATION = 'toggleDictation',
  TOGGLE_EARCONS = 'toggleEarcons',
  TOGGLE_KEYBOARD_HELP = 'toggleKeyboardHelp',
  TOGGLE_SCREEN = 'toggleScreen',
  TOGGLE_SEARCH_WIDGET = 'toggleSearchWidget',
  TOGGLE_SELECTION = 'toggleSelection',
  TOGGLE_SEMANTICS = 'toggleSemantics',
  TOGGLE_SPEECH_ON_OR_OFF = 'toggleSpeechOnOrOff',
  TOGGLE_STICKY_MODE = 'toggleStickyMode',
  TOP = 'top',
  VIEW_GRAPHIC_AS_BRAILLE = 'viewGraphicAsBraille',
}

/**
 * List of categories for the commands.
 * Note that the values here must correspond to the message resource tag for the
 * category.
 */
export enum CommandCategory {
  ACTIONS = 'actions',
  CONTROLLING_SPEECH = 'controlling_speech',
  HELP_COMMANDS = 'help_commands',
  INFORMATION = 'information',
  JUMP_COMMANDS = 'jump_commands',
  MODIFIER_KEYS = 'modifier_keys',
  NAVIGATION = 'navigation',
  OVERVIEW = 'overview',
  TABLES = 'tables',
  // The following categories are not displayed in the ChromeVox menus:
  BRAILLE = 'braille',
  DEVELOPER = 'developer',
  NO_CATEGORY = 'no_category',
}