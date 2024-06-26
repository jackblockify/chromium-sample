
#include <optional>

#include "base/memory/raw_ptr.h"
#include "ui/base/models/simple_menu_model.h"

class Browser;
class PrefService;

class VPNMenuModel : public ui::SimpleMenuModel,
                          public ui::SimpleMenuModel::Delegate{

 private:
  FRIEND_TEST_ALL_PREFIXES(BraveVPNMenuModelUnitTest, TrayIconEnabled);
  FRIEND_TEST_ALL_PREFIXES(BraveVPNMenuModelUnitTest, TrayIconDisabled);
  FRIEND_TEST_ALL_PREFIXES(BraveVPNMenuModelUnitTest, ToolbarVPNButton);

  // ui::SimpleMenuModel::Delegate override:
  void ExecuteCommand(int command_id, int event_flags) override;

  void Build();
  bool IsBraveVPNButtonVisible() const;
  std::optional<bool> tray_icon_enabled_for_testing_;
  raw_ptr<PrefService> profile_prefs_ = nullptr;
  raw_ptr<Browser> browser_ = nullptr;
}