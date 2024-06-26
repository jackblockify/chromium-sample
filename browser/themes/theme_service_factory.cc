// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_service_factory.h"

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"
#include "ui/base/mojom/themes.mojom.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/themes/theme_helper_win.h"
#endif

// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/themes/theme_service_aura_linux.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "ui/linux/linux_ui_factory.h"
#endif

namespace {

const ThemeHelper& GetThemeHelper() {
#if BUILDFLAG(IS_WIN)
  using ThemeHelper = ThemeHelperWin;
#endif

  static base::NoDestructor<std::unique_ptr<ThemeHelper>> theme_helper(
      std::make_unique<ThemeHelper>());
  return **theme_helper;
}

BASE_FEATURE(kProfileBasedThemeService,
             "ProfileBasedThemeService",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace

// static
ThemeService* ThemeServiceFactory::GetForProfile(Profile* profile) {
  TRACE_EVENT0("loading", "ThemeServiceFactory::GetForProfile");
  if (base::FeatureList::IsEnabled(kProfileBasedThemeService)) {
    if (!profile->theme_service()) {
      profile->set_theme_service(static_cast<ThemeService*>(
          GetInstance()->GetServiceForBrowserContext(profile, true)));
    }
    return profile->theme_service().value();
  }
  return static_cast<ThemeService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
const extensions::Extension* ThemeServiceFactory::GetThemeForProfile(
    Profile* profile) {
  ThemeService* theme_service = GetForProfile(profile);
  if (!theme_service->UsingExtensionTheme())
    return nullptr;

  return extensions::ExtensionRegistry::Get(profile)
      ->enabled_extensions()
      .GetByID(theme_service->GetThemeID());
}

// static
ThemeServiceFactory* ThemeServiceFactory::GetInstance() {
  static base::NoDestructor<ThemeServiceFactory> instance;
  return instance.get();
}

ThemeServiceFactory::ThemeServiceFactory()
    : ProfileKeyedServiceFactory(
          "ThemeService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(extensions::ExtensionRegistryFactory::GetInstance());
  DependsOn(extensions::ExtensionPrefsFactory::GetInstance());
  DependsOn(extensions::ExtensionSystemFactory::GetInstance());
}

ThemeServiceFactory::~ThemeServiceFactory() = default;

KeyedService* ThemeServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
#if BUILDFLAG(IS_LINUX)
  using ThemeService = ThemeServiceAuraLinux;
#endif

  auto provider = std::make_unique<ThemeService>(static_cast<Profile*>(profile),
                                                 GetThemeHelper());
  provider->Init();
  return provider.release();
}

void ThemeServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  ui::SystemTheme default_system_theme = ui::SystemTheme::kDefault;
#if BUILDFLAG(IS_LINUX)
  default_system_theme = ui::GetDefaultSystemTheme();
#endif
  registry->RegisterIntegerPref(prefs::kSystemTheme,
                                static_cast<int>(default_system_theme));
#endif
  registry->RegisterFilePathPref(prefs::kCurrentThemePackFilename,
                                 base::FilePath());
  registry->RegisterStringPref(prefs::kCurrentThemeID,
                               ThemeHelper::kDefaultThemeID);
  registry->RegisterIntegerPref(prefs::kAutogeneratedThemeColor, 0);
  registry->RegisterIntegerPref(prefs::kPolicyThemeColor, SK_ColorTRANSPARENT);
  registry->RegisterIntegerPref(
      prefs::kBrowserColorScheme,
      static_cast<int>(ThemeService::BrowserColorScheme::kSystem),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kUserColor, SK_ColorTRANSPARENT,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kBrowserColorVariant,
      static_cast<int>(ui::mojom::BrowserColorVariant::kSystem),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kGrayscaleThemeEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kBrowserFollowsSystemThemeColors,
                                BUILDFLAG(IS_CHROMEOS));
}

bool ThemeServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

void ThemeServiceFactory::BrowserContextDestroyed(
    content::BrowserContext* browser_context) {
  Profile::FromBrowserContext(browser_context)->set_theme_service(nullptr);
  BrowserContextKeyedServiceFactory::BrowserContextDestroyed(browser_context);
}
