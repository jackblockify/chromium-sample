// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/pin_options_button.h"

#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/vector_icons.h"

namespace {

constexpr int kCheckIconSize = 16;

std::u16string GetCommandIdLabel(int command_id) {
  switch (command_id) {
    case PinOptionsButton::CommandId::CHOOSE_SIX_DIGIT_PIN:
      return l10n_util::GetStringUTF16(IDS_WEBAUTHN_GPM_PIN_OPTION_NUMBERS);
    case PinOptionsButton::CommandId::CHOOSE_ARBITRARY_PIN:
      return l10n_util::GetStringUTF16(
          IDS_WEBAUTHN_GPM_PIN_OPTION_ALPHANUMERIC);
    default:
      NOTREACHED_IN_MIGRATION();
      return u"";
  }
}

}  // namespace

PinOptionsButton::PinOptionsButton(const std::u16string& label,
                                   CommandId checked_command_id,
                                   base::RepeatingCallback<void(bool)> callback)
    : views::MdTextButtonWithDownArrow(
          base::BindRepeating(&PinOptionsButton::ButtonPressed,
                              base::Unretained(this)),
          label),
      callback_(std::move(callback)),
      menu_model_(std::make_unique<ui::SimpleMenuModel>(this)) {
  GetViewAccessibility().SetName(label);
  SetFocusBehavior(FocusBehavior::ALWAYS);

  for (int command_id = 0; command_id < CommandId::COMMAND_ID_COUNT;
       command_id++) {
    const std::u16string command_label = GetCommandIdLabel(command_id);
    if (command_id == checked_command_id) {
      menu_model_->AddItemWithIcon(
          command_id, command_label,
          ui::ImageModel::FromVectorIcon(views::kMenuCheckIcon,
                                         ui::kColorMenuIcon, kCheckIconSize));
    } else {
      menu_model_->AddItem(command_id, command_label);
    }
  }
}

PinOptionsButton::~PinOptionsButton() = default;

void PinOptionsButton::ButtonPressed() {
  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu_model_.get(),
      views::MenuRunner::COMBOBOX | views::MenuRunner::HAS_MNEMONICS);
  menu_runner_->RunMenuAt(
      GetWidget(), /*button_controller=*/nullptr, GetBoundsInScreen(),
      views::MenuAnchorPosition::kTopLeft, ui::MENU_SOURCE_NONE);
}

void PinOptionsButton::ExecuteCommand(int command_id, int event_flags) {
  switch (command_id) {
    case CommandId::CHOOSE_SIX_DIGIT_PIN:
      callback_.Run(/*is_arbitrary=*/false);
      break;
    case CommandId::CHOOSE_ARBITRARY_PIN:
      callback_.Run(/*is_arbitrary=*/true);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      return;
  }
}

BEGIN_METADATA(PinOptionsButton)
END_METADATA
