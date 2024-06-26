// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export type VoicePackStatus = VoicePackServerResponseSuccess|
    VoicePackServerResponseError|VoicePackServerResponseParsingError;

const STATUS_SUCCESS = 'Successful response';
const STATUS_FAILURE = 'Unsuccessful response';
const PARSING_ERROR = 'Cannot parse LanguagePackManager response';

// Representation of server-side LanguagePackManager state
interface VoicePackServerResponseSuccess {
  id: 'Successful response';
  code: VoicePackServerStatusSuccessCode;
}
interface VoicePackServerResponseError {
  id: 'Unsuccessful response';
  code: VoicePackServerStatusErrorCode;
}

interface VoicePackServerResponseParsingError {
  id: 'Cannot parse LanguagePackManager response';
  code: 'ParseError';
}

export function isVoicePackStatusSuccess(status?: VoicePackStatus):
    status is VoicePackServerResponseSuccess {
  if (status === undefined) {
    return false;
  }
  return status.id === STATUS_SUCCESS;
}

export function isVoicePackStatusError(status?: VoicePackStatus):
    status is VoicePackServerResponseError {
  if (status === undefined) {
    return false;
  }

  return status.id === STATUS_FAILURE;
}

// Representation of server-side LanguagePackManager state. Roughly corresponds
// to InstallationState in read_anything.mojom
export enum VoicePackServerStatusSuccessCode {
  NOT_INSTALLED,  // Available to be downloaded but not installed
  INSTALLING,     // Currently installing
  INSTALLED,      // Is downloaded onto the device
}

// Representation of server-side LanguagePackManager state. Roughly corresponds
// to ErrorCode in read_anything.mojom. We treat many of these errors in the
// same way, but these are the states that the server sends us.
export enum VoicePackServerStatusErrorCode {
  OTHER,                 // A catch all error
  WRONG_ID,              // If no language pack for this language
  NEED_REBOOT,           // Error installing and a reboot should help
  ALLOCATION,            // Error due to not enough memory
  UNSUPPORTED_PLATFORM,  // Donloads not supported on this platform
}

// Our client-side representation tracking voice-pack states.
export enum VoiceClientSideStatusCode {
  NOT_INSTALLED,         // Available to be downloaded but not installed
  SENT_INSTALL_REQUEST,  // We sent an install request
  SENT_INSTALL_REQUEST_ERROR_RETRY,  // We sent an install request retrying a
                                     // previously failed download
  INSTALLED_AND_UNAVAILABLE,  // The server says voice is on disk, but it's not
                              // available to the local speechSynthesis API yet
  AVAILABLE,  // The voice is installed and available to be used by the local
              // speechSynthesis API
  ERROR_INSTALLING,          // Couldn't install
  INSTALL_ERROR_ALLOCATION,  // Couldn't install due to not enough memory
}

// These strings are not localized and will be in English, even for non-English
// Natural and eSpeak voices.
const NATURAL_STRING_IDENTIFIER = '(Natural)';
const ESPEAK_STRING_IDENTIFIER = 'eSpeak';

export function isNatural(voice: SpeechSynthesisVoice) {
  return voice.name.includes(NATURAL_STRING_IDENTIFIER);
}

export function isEspeak(voice: SpeechSynthesisVoice) {
  return voice.name.includes(ESPEAK_STRING_IDENTIFIER);
}

export function createInitialListOfEnabledLanguages(
    browserOrPageBaseLang: string, storedLanguagesPref: string[],
    availableLangs: string[], langOfDefaultVoice: string|undefined): string[] {
  const initialAvailableLanguages: Set<string> = new Set();

  // Add stored prefs to initial list of enabled languages
  for (const lang of storedLanguagesPref) {
    // Find the version of the lang/locale that maps to a language
    const matchingLang =
        convertLangToAnAvailableLangIfPresent(lang, availableLangs);
    if (matchingLang) {
      initialAvailableLanguages.add(matchingLang);
    }
  }

  // Add browserOrPageBaseLang to initial list of enabled languages
  // If there's no locale/base-lang already matching in
  // initialAvailableLanguages, then add one
  const browserPageLangAlreadyPresent = [...initialAvailableLanguages].some(
      lang => lang.startsWith(browserOrPageBaseLang));
  if (!browserPageLangAlreadyPresent) {
    const matchingLangOfBrowserLang = convertLangToAnAvailableLangIfPresent(
        browserOrPageBaseLang, availableLangs);
    if (matchingLangOfBrowserLang) {
      initialAvailableLanguages.add(matchingLangOfBrowserLang);
    }
  }

  // If initialAvailableLanguages is still empty, add the default voice
  // language
  if (initialAvailableLanguages.size === 0) {
    if (langOfDefaultVoice) {
      initialAvailableLanguages.add(langOfDefaultVoice);
    }
  }

  return [...initialAvailableLanguages];
}

export function convertLangToAnAvailableLangIfPresent(
    langOrLocale: string, availableLangs: string[],
    allowCurrentLanguageIfExists: boolean = true): string|undefined {
  // Convert everything to lower case
  langOrLocale = langOrLocale.toLowerCase();
  availableLangs = availableLangs.map(lang => lang.toLowerCase());

  if (allowCurrentLanguageIfExists && availableLangs.includes(langOrLocale)) {
    return langOrLocale;
  }

  const baseLang = extractBaseLang(langOrLocale);
  if (allowCurrentLanguageIfExists && availableLangs.includes(baseLang)) {
    return baseLang;
  }

  // See if there are any matching available locales we can default to
  const matchingLocales: string[] = availableLangs.filter(
      availableLang => extractBaseLang(availableLang) === baseLang);
  if (matchingLocales && matchingLocales[0]) {
    return matchingLocales[0];
  }

  // If all else fails, try the browser language.
  const defaultLanguage =
      chrome.readingMode.defaultLanguageForSpeech.toLowerCase();
  if (availableLangs.includes(defaultLanguage)) {
    return defaultLanguage;
  }

  // Try the browser language converted to a locale.
  const convertedDefaultLanguage =
      convertUnsupportedBaseLangToSupportedLocale(defaultLanguage);
  if (convertedDefaultLanguage &&
      availableLangs.includes(convertedDefaultLanguage)) {
    return convertedDefaultLanguage;
  }

  return undefined;
}

// The following possible values of "status" is a union of enum values of
// enum InstallationState and enum ErrorCode in read_anything.mojom
export function mojoVoicePackStatusToVoicePackStatusEnum(
    mojoPackStatus: string): VoicePackStatus {
  if (mojoPackStatus === 'kNotInstalled') {
    return {
      id: STATUS_SUCCESS,
      code: VoicePackServerStatusSuccessCode.NOT_INSTALLED,
    };
  } else if (mojoPackStatus === 'kInstalling') {
    return {
      id: STATUS_SUCCESS,
      code: VoicePackServerStatusSuccessCode.INSTALLING,
    };
  } else if (mojoPackStatus === 'kInstalled') {
    return {
      id: STATUS_SUCCESS,
      code: VoicePackServerStatusSuccessCode.INSTALLED,
    };
  } else if (mojoPackStatus === 'kOther' || mojoPackStatus === 'kUnknown') {
    return {id: STATUS_FAILURE, code: VoicePackServerStatusErrorCode.OTHER};
  } else if (mojoPackStatus === 'kWrongId') {
    return {id: STATUS_FAILURE, code: VoicePackServerStatusErrorCode.WRONG_ID};
  } else if (mojoPackStatus === 'kNeedReboot') {
    return {
      id: STATUS_FAILURE,
      code: VoicePackServerStatusErrorCode.NEED_REBOOT,
    };
  } else if (mojoPackStatus === 'kAllocation') {
    return {
      id: STATUS_FAILURE,
      code: VoicePackServerStatusErrorCode.ALLOCATION,
    };
  } else if (mojoPackStatus === 'kUnsupportedPlatform') {
    return {
      id: STATUS_FAILURE,
      code: VoicePackServerStatusErrorCode.UNSUPPORTED_PLATFORM,
    };
  } else {
    return {id: PARSING_ERROR, code: 'ParseError'};
  }
}

// The ChromeOS VoicePackManager labels some voices by locale, and some by
// base-language. The request for each needs to be exact, so this function
// converts a locale or language into the code the VoicePackManager expects.
// This is based on the VoicePackManager code here:
// https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/language_packs/language_pack_manager.cc;l=346;drc=31e516b25930112df83bf09d3d2a868200ecbc6d
export function convertLangOrLocaleForVoicePackManager(langOrLocale: string):
    string|undefined {
  langOrLocale = langOrLocale.toLowerCase();
  if (PACK_MANAGER_SUPPORTED_LANGS_AND_LOCALES.has(langOrLocale)) {
    return langOrLocale;
  }

  if (!isBaseLang(langOrLocale)) {
    const baseLang = langOrLocale.substring(0, langOrLocale.indexOf('-'));
    if (PACK_MANAGER_SUPPORTED_LANGS_AND_LOCALES.has(baseLang)) {
      return baseLang;
    }
    const locale = convertUnsupportedBaseLangToSupportedLocale(baseLang);
    if (locale) {
      return locale;
    }
  }

  const locale = convertUnsupportedBaseLangToSupportedLocale(langOrLocale);
  if (locale) {
    return locale;
  }

  return undefined;
}

export function convertLangOrLocaleToExactVoicePackLocale(langOrLocale: string):
    string|undefined {
  const possibleConvertedLang =
      convertLangOrLocaleForVoicePackManager(langOrLocale);
  if (!possibleConvertedLang) {
    return possibleConvertedLang;
  }

  return [...AVAILABLE_GOOGLE_TTS_LOCALES].find(
      locale => locale.startsWith(possibleConvertedLang.toLowerCase()));
}

function convertUnsupportedBaseLangToSupportedLocale(baseLang: string): string|
    undefined {
  // Check if it's a base lang that supports a locale. These are the only
  // languages that have locales in the Pack Manager per the code link above.
  if (['en', 'es', 'pt'].includes(baseLang)) {
    // TODO (b/335691447) Convert from base-lang to locale based on browser
    // prefs. For now, just default to arbitrary locales.
    if (baseLang === 'en') {
      return 'en-us';
    }
    if (baseLang === 'es') {
      return 'es-es';
    }
    if (baseLang === 'pt') {
      return 'pt-br';
    }
  }
  return undefined;
}

// Returns true if input is base lang, and false if it's a locale
function isBaseLang(langOrLocale: string): boolean {
  return !langOrLocale.includes('-');
}

function extractBaseLang(langOrLocale: string): string {
  if (isBaseLang(langOrLocale)) {
    return langOrLocale;
  }
  return langOrLocale.substring(0, langOrLocale.indexOf('-'));
}

// These are from the Pack Manager. Values should be kept in sync with the code
// link above.
export const PACK_MANAGER_SUPPORTED_LANGS_AND_LOCALES = new Set([
  'bn',    'cs', 'da',  'de', 'el', 'en-au', 'en-gb', 'en-us', 'es-es',
  'es-us', 'fi', 'fil', 'fr', 'hi', 'hu',    'id',    'it',    'ja',
  'km',    'ko', 'nb',  'ne', 'nl', 'pl',    'pt-br', 'pt-pt', 'si',
  'sk',    'sv', 'th',  'tr', 'uk', 'vi',    'yue',
]);

// These are the locales based on PACK_MANAGER_SUPPORTED_LANGS_AND_LOCALES, but
// for the actual Google TTS locales that can be installed on ChromeOS. While
// we can use the languages in PACK_MANAGER_SUPPORTED_LANGS_AND_LOCALES to
// download a voice pack, the voice pack language code will be returned in
// the locale format, as in AVAILABLE_GOOGLE_TTS_LOCALES, which means the
// previously toggled language item won't match the language item associated
// with the downloaded pack.
export const AVAILABLE_GOOGLE_TTS_LOCALES = new Set([
  'bn-bd', 'cs-cz', 'da-dk', 'de-de', 'el-gr',  'en-au',  'en-gb',
  'en-us', 'es-es', 'es-us', 'fi-fi', 'fil-ph', 'fr-fr',  'hi-in',
  'hu-hu', 'id-id', 'it-it', 'ja-jp', 'km-kh',  'ko-kr',  'nb-no',
  'ne-np', 'nl-nl', 'pl-pl', 'pt-br', 'pt-pt',  'si-lk',  'sk-sk',
  'sv-se', 'th-th', 'tr-tr', 'uk-ua', 'vi-vn',  'yue-hk',
]);

export function areVoicesEqual(
    voice1: SpeechSynthesisVoice|null,
    voice2: SpeechSynthesisVoice|null): boolean {
  if (!voice1 || !voice2) {
    return false;
  }
  return voice1.default === voice2.default && voice1.lang === voice2.lang &&
      voice1.localService === voice2.localService &&
      voice1.name === voice2.name && voice1.voiceURI === voice2.voiceURI;
}
