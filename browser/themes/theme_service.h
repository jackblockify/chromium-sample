// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THEMES_THEME_SERVICE_H_
#define CHROME_BROWSER_THEMES_THEME_SERVICE_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/themes/theme_helper.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_id.h"
#include "ui/base/mojom/themes.mojom.h"
#include "ui/base/theme_provider.h"
#include "ui/color/system_theme.h"

class BrowserThemePack;
class CustomThemeSupplier;
class Profile;
class ThemeServiceObserver;
class ThemeSyncableService;

namespace extensions {
class Extension;
}  // namespace extensions

namespace theme_service_internal {
class ThemeServiceTest;
}  // namespace theme_service_internal

namespace ui {
class ColorProvider;
}  // namespace ui

class BrowserThemeProviderDelegate {
 public:
  virtual CustomThemeSupplier* GetThemeSupplier() const = 0;
  virtual bool ShouldUseCustomFrame() const = 0;
};

// A theme consists of a set of colors and images, including the NTP background
// image. See CustomThemeSupplier for details. There are multiple sources for
// themes, including extensions, NTP, the system theme, and policy.
// TODO(https://crbug.com/341787825): When the NTP generates a custom background
// image, the color is stored in ThemeService but the background image is stored
// in NtpCustomBackgroundService. This divergence from other theme sources
// introduces complexity and fragility.
class ThemeService : public KeyedService, public BrowserThemeProviderDelegate {
 public:
  // This is stored as an integer in the profile prefs, so entries should not be
  // renumbered and numeric values should never be reused.
  enum class BrowserColorScheme {
    kSystem = 0,
    kLight = 1,
    kDark = 2,
  };

  // This class keeps track of the number of existing |ThemeReinstaller|
  // objects. When that number reaches 0 then unused themes will be deleted.
  class ThemeReinstaller {
   public:
    ThemeReinstaller(Profile* profile, base::OnceClosure installer);
    ThemeReinstaller(const ThemeReinstaller&) = delete;
    ThemeReinstaller& operator=(const ThemeReinstaller&) = delete;
    ~ThemeReinstaller();

    void Reinstall();

   private:
    base::OnceClosure installer_;
    const raw_ptr<ThemeService> theme_service_;
  };

  // Constant ID to use for all autogenerated themes.
  static const char kAutogeneratedThemeID[];
  // Constant ID to use for all user color themes.
  static const char kUserColorThemeID[];

  // Creates a ThemeProvider with a custom theme supplier specified via
  // |delegate|. The return value must not outlive |profile|'s ThemeService.
  static std::unique_ptr<ui::ThemeProvider> CreateBoundThemeProvider(
      Profile* profile,
      BrowserThemeProviderDelegate* delegate);

  ThemeService(Profile* profile, const ThemeHelper& theme_helper);
  ThemeService(const ThemeService&) = delete;
  ThemeService& operator=(const ThemeService&) = delete;
  ~ThemeService() override;

  void Init();

  // KeyedService:
  void Shutdown() override;

  // Overridden from BrowserThemeProviderDelegate:
  CustomThemeSupplier* GetThemeSupplier() const override;
  bool ShouldUseCustomFrame() const override;

  // Set the current theme to the theme defined in |extension|.
  // |extension| must already be added to this profile's
  // ExtensionService.
  void SetTheme(const extensions::Extension* extension);

  // Similar to SetTheme, but doesn't show an undo infobar.
  void RevertToExtensionTheme(const std::string& extension_id);

  // Sets the platform theme based on `system_theme`.
  virtual void UseTheme(ui::SystemTheme system_theme);

  // Reset the theme to default.
  virtual void UseDefaultTheme();

  // Toggle whether the browser follows its own theme or from the OS. For now,
  // this is distinct from the ui::SystemTheme.
  virtual void UseDeviceTheme(bool follow);

  // Returns true if the browser should follow the OS theme.
  virtual bool UsingDeviceTheme() const;

  // Set the current theme to the system theme. On some platforms, the system
  // theme is the default theme.
  virtual void UseSystemTheme();

  // Returns true if the default theme and system theme are not the same on
  // this platform.
  virtual bool IsSystemThemeDistinctFromDefaultTheme() const;

  // Forwards to ThemeProviderBase::IsDefaultTheme().
  // Virtual for testing.
  virtual bool UsingDefaultTheme() const;

  // Whether we are using the system theme. On Linux, the system theme is the
  // GTK or QT themes, not the "Classic" theme.
  virtual bool UsingSystemTheme() const;

  // Forwards to ThemeProviderBase::IsExtensionTheme().
  // Virtual for testing.
  virtual bool UsingExtensionTheme() const;

  // Forwards to ThemeProviderBase::IsAutogeneratedTheme().
  // Virtual for testing.
  virtual bool UsingAutogeneratedTheme() const;

  // Whether current theme colors are enforced through a policy.
  // Virtual for testing.
  virtual bool UsingPolicyTheme() const;

  // Gets the id of the last installed theme. (The theme may have been further
  // locally customized.)
  virtual std::string GetThemeID() const;

  // Uninstall theme extensions which are no longer in use.
  void RemoveUnusedThemes();

  // Returns the syncable service for syncing theme. The returned service is
  // owned by |this| object.
  virtual ThemeSyncableService* GetThemeSyncableService() const;

  // Gets the ThemeProvider for |profile|. This will be different for an
  // incognito profile and its original profile, even though both profiles use
  // the same ThemeService.
  //
  // Before using this function, consider if the caller is in a rooted UI tree.
  // If it is, strongly favor referring to the conceptual roots for a
  // ThemeProvider. For Views, this is the Widget. For Cocoa, this is the
  // AppControl.
  static const ui::ThemeProvider& GetThemeProviderForProfile(Profile* profile);
  static CustomThemeSupplier* GetThemeSupplierForProfile(Profile* profile);

  // Gets the ColorProvider for this ThemeService.
  //
  // Before using this function, consider if the caller is in a rooted UI tree.
  // If it is, strongly favor referring to the conceptual roots for a
  // ColorProvider. For Views, this is the Widget. For Cocoa, this is the
  // AppControl.
  ui::ColorProvider* GetColorProvider();

  // Builds an autogenerated theme from a given |color| and applies it.
  virtual void BuildAutogeneratedThemeFromColor(SkColor color);

  // Builds an autogenerated theme from a given |color| and applies it.
  virtual void BuildAutogeneratedThemeFromColor(SkColor color,
                                                bool store_in_prefs);

  // Returns the theme color for an autogenerated theme.
  virtual SkColor GetAutogeneratedThemeColor() const;

  // Builds an autogenerated theme from the applied policy color and applies it.
  virtual void BuildAutogeneratedPolicyTheme();

  // Returns the theme color for the current policy theme.
  virtual SkColor GetPolicyThemeColor() const;

  // Sets the browser color scheme preference.
  virtual void SetBrowserColorScheme(BrowserColorScheme color_scheme);

  // Gets the browser color scheme preference.
  virtual BrowserColorScheme GetBrowserColorScheme() const;

  // Sets/gets the browser user color preference.
  virtual void SetUserColor(std::optional<SkColor> user_color);
  virtual std::optional<SkColor> GetUserColor() const;

  // Sets/gets the browser kSchemeVariant preference.
  void SetBrowserColorVariant(ui::mojom::BrowserColorVariant color_variant);
  virtual ui::mojom::BrowserColorVariant GetBrowserColorVariant() const;

  // Convenience method that allows setting both the kUserColor and
  // kSchemeVariant before propagating theme update notifications.
  virtual void SetUserColorAndBrowserColorVariant(
      SkColor user_color,
      ui::mojom::BrowserColorVariant color_variant);

  // Sets/gets the browser grayscale theme preference.
  virtual void SetIsGrayscale(bool is_grayscale);
  virtual bool GetIsGrayscale() const;

  // Returns true if the current theme is the "baseline" blue theme.
  virtual bool GetIsBaseline() const;

  // Returns |ThemeService::ThemeReinstaller| for the current theme.
  std::unique_ptr<ThemeService::ThemeReinstaller>
  BuildReinstallerForCurrentTheme();

  // Virtual for testing.
  virtual void AddObserver(ThemeServiceObserver* observer);

  void RemoveObserver(ThemeServiceObserver* observer);

  const ThemeHelper& theme_helper_for_testing() const { return *theme_helper_; }

  // Don't create "Cached Theme.pak" in the extension directory, for testing.
  static void DisableThemePackForTesting();

 protected:
  // Set a custom default theme instead of the normal default theme.
  virtual void SetCustomDefaultTheme(
      scoped_refptr<CustomThemeSupplier> theme_supplier);

  // Returns the theme service type that should be used on startup.
  virtual ui::SystemTheme GetDefaultSystemTheme() const;

  // Clears override fields and saves the dictionary.
  virtual void ClearThemeData(bool clear_ntp_background);

  // Initialize current theme state data from preferences.
  virtual void InitFromPrefs();

  // Let all the browser views know that themes have changed.
  virtual void NotifyThemeChanged();

  // If there is an inconsistency in preferences, change preferences to a
  // consistent state.
  virtual void FixInconsistentPreferencesIfNeeded();

  Profile* profile() const { return profile_; }

  void set_ready() { ready_ = true; }

  // True if the theme service is ready to be used.
  // TODO(pkotwicz): Add DCHECKS to the theme service's getters once
  // ThemeSource no longer uses the ThemeService when it is not ready.
  bool ready_ = false;

 private:
  // This class implements ui::ThemeProvider on behalf of ThemeHelper and
  // keeps track of the incognito state and CustemThemeSupplier for the calling
  // code.
  class BrowserThemeProvider : public ui::ThemeProvider {
   public:
    BrowserThemeProvider(const ThemeHelper& theme_helper,
                         bool incognito,
                         const BrowserThemeProviderDelegate* delegate);
    BrowserThemeProvider(const BrowserThemeProvider&) = delete;
    BrowserThemeProvider& operator=(const BrowserThemeProvider&) = delete;
    ~BrowserThemeProvider() override;

    // Overridden from ui::ThemeProvider:
    gfx::ImageSkia* GetImageSkiaNamed(int id) const override;
    color_utils::HSL GetTint(int original_id) const override;
    int GetDisplayProperty(int id) const override;
    bool ShouldUseNativeFrame() const override;
    bool HasCustomImage(int id) const override;
    base::RefCountedMemory* GetRawData(
        int id,
        ui::ResourceScaleFactor scale_factor) const override;

   private:
    CustomThemeSupplier* GetThemeSupplier() const;

    const raw_ref<const ThemeHelper> theme_helper_;
    bool incognito_;
    raw_ptr<const BrowserThemeProviderDelegate> delegate_;
  };
  friend class BrowserThemeProvider;
  friend class theme_service_internal::ThemeServiceTest;

  // virtual for testing.
  virtual void DoSetTheme(const extensions::Extension* extension,
                          bool suppress_infobar);

  // Called when the extension service is ready.
  void OnExtensionServiceReady();

  // Migrate the theme to the new theme pack schema by recreating the data pack
  // from the extension.
  void MigrateTheme();

  // Replaces the current theme supplier with a new one and calls
  // StopUsingTheme() or StartUsingTheme() as appropriate.
  void SwapThemeSupplier(scoped_refptr<CustomThemeSupplier> theme_supplier);

  // Implementation of SetTheme() (and the fallback from InitFromPrefs() in
  // case we don't have a theme pack). |new_theme| indicates whether this is a
  // newly installed theme or a migration.
  void BuildFromExtension(const extensions::Extension* extension,
                          bool new_theme);

  // Callback when |pack| has finished or failed building.
  void OnThemeBuiltFromExtension(const extensions::ExtensionId& extension_id,
                                 scoped_refptr<BrowserThemePack> pack,
                                 bool new_theme);

  // Handles theme color policy pref updates. if policy value contains valid
  // color(s), sets browser theme accordingly.
  void HandlePolicyColorUpdate();

  // Functions that modify theme prefs.
  void ClearThemePrefs();
  void SetThemePrefsForExtension(const extensions::Extension* extension);
  void SetThemePrefsForColor(SkColor color);

  bool DisableExtension(const std::string& extension_id);

  raw_ptr<Profile> profile_;
  PrefChangeRegistrar pref_change_registrar_;

  const raw_ref<const ThemeHelper> theme_helper_;
  scoped_refptr<CustomThemeSupplier> theme_supplier_;

  // The id of the theme extension which has just been installed but has not
  // been loaded yet. The theme extension with |installed_pending_load_id_| may
  // never be loaded if the install is due to updating a disabled theme.
  // |pending_install_id_| should be set to |kDefaultThemeID| if there are no
  // recently installed theme extensions
  std::string installed_pending_load_id_ = ThemeHelper::kDefaultThemeID;

  // The number of infobars currently displayed.
  int number_of_reinstallers_ = 0;

  // Configuring ThemeService into a desired theme state may involve the
  // configuration of multiple theme prefs. However updating a single theme pref
  // may result in requests to propagate theme update notifications. This flag
  // is used to avoid propagating updates until all theme prefs have been set
  // to their desired values.
  bool should_suppress_theme_updates_ = false;

  // Declared before |theme_syncable_service_|, because ThemeSyncableService
  // removes itself from the |observers_| list on destruction.
  base::ObserverList<ThemeServiceObserver> observers_;

  std::unique_ptr<ThemeSyncableService> theme_syncable_service_;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  class ThemeObserver;
  std::unique_ptr<ThemeObserver> theme_observer_;
#endif

  BrowserThemeProvider original_theme_provider_;
  BrowserThemeProvider incognito_theme_provider_;

  // Allows us to cancel building a theme pack from an extension.
  base::CancelableTaskTracker build_extension_task_tracker_;

  // The ID of the theme that's currently being built on a different thread.
  // We hold onto this just to be sure not to uninstall the extension view
  // RemoveUnusedThemes while it's still being built.
  std::string building_extension_id_;

  base::WeakPtrFactory<ThemeService> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_THEMES_THEME_SERVICE_H_
