// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_generation_popup_view_views.h"

#include <algorithm>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/passwords/password_generation_popup_controller.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/passwords/views_utils.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/vector_icons.h"
#include "ui/views/widget/widget.h"

namespace {

using password_manager::features::PasswordGenerationVariation;

// The max width prevents the popup from growing too much when the password
// field is too long.
constexpr int kPasswordGenerationMaxWidth = 480;

// The default icon size used in the password generation drop down.
constexpr int kIconSize = 16;

// Adds space between child views. The `view`'s LayoutManager  must be a
// BoxLayout.
void AddSpacerWithSize(int spacer_width, bool resize, views::View* view) {
  auto spacer = std::make_unique<views::View>();
  spacer->SetPreferredSize(gfx::Size(spacer_width, /*height=*/1));
  static_cast<views::BoxLayout*>(view->GetLayoutManager())
      ->SetFlexForView(view->AddChildView(std::move(spacer)),
                       /*flex=*/resize ? 1 : 0,
                       /*use_min_size=*/true);
}

// Returns the message id of the help text which may differ depending on
// specific variation of `kPasswordGenerationExperiment` feature enabled.
int GetHelpTextMessageId() {
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kPasswordGenerationExperiment)) {
    return IDS_PASSWORD_GENERATION_PROMPT_GOOGLE_PASSWORD_MANAGER;
  }

  switch (
      password_manager::features::kPasswordGenerationExperimentVariationParam
          .Get()) {
    case PasswordGenerationVariation::kTrustedAdvice:
      return IDS_PASSWORD_GENERATION_HELP_TEXT_TRUSTED_ADVICE;
    case PasswordGenerationVariation::kSafetyFirst:
      return IDS_PASSWORD_GENERATION_HELP_TEXT_SAFETY_FIRST;
    case PasswordGenerationVariation::kTrySomethingNew:
      return IDS_PASSWORD_GENERATION_HELP_TEXT_TRY_SOMETHING_NEW;
    case PasswordGenerationVariation::kConvenience:
      return IDS_PASSWORD_GENERATION_HELP_TEXT_CONVENIENCE;
    case PasswordGenerationVariation::kCrossDevice:
      return IDS_PASSWORD_GENERATION_HELP_TEXT;
    default:
      return IDS_PASSWORD_GENERATION_PROMPT_GOOGLE_PASSWORD_MANAGER;
  }
}

std::unique_ptr<views::FlexLayoutView> CreateLabelWithCheckIcon(
    const std::u16string& label_text) {
  auto label_with_icon = std::make_unique<views::FlexLayoutView>();

  auto icon = std::make_unique<NonAccessibleImageView>();
  icon->SetImage(ui::ImageModel::FromVectorIcon(
      views::kMenuCheckIcon, ui::kColorIconSecondary, kIconSize));
  label_with_icon->AddChildView(std::move(icon));

  auto spacer = std::make_unique<views::View>();
  spacer->SetPreferredSize(gfx::Size(
      autofill::PopupBaseView::ArrowHorizontalMargin(), /*height=*/1));
  label_with_icon->AddChildView(std::move(spacer));

  label_with_icon->AddChildView(
      std::make_unique<views::Label>(label_text, CONTEXT_DIALOG_BODY_TEXT_SMALL,
                                     views::style::STYLE_SECONDARY));

  return label_with_icon;
}

// Creates help text listing benefits of password generation in bullet points.
std::unique_ptr<views::View> CreateCrossDeviceFooter(
    const std::u16string& primary_account_email) {
  auto cross_device_footer = std::make_unique<views::View>();

  auto* layout =
      cross_device_footer->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  cross_device_footer->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_PASSWORD_GENERATION_BENEFITS),
      CONTEXT_DIALOG_BODY_TEXT_SMALL, views::style::STYLE_SECONDARY));
  cross_device_footer->AddChildView(CreateLabelWithCheckIcon(
      l10n_util::GetStringUTF16(IDS_PASSWORD_GENERATION_CROSS_DEVICE)));
  cross_device_footer->AddChildView(CreateLabelWithCheckIcon(
      l10n_util::GetStringUTF16(IDS_PASSWORD_GENERATION_SECURITY)));
  cross_device_footer->AddChildView(CreateLabelWithCheckIcon(
      l10n_util::GetStringUTF16(IDS_PASSWORD_GENERATION_PROACTIVE_CHECK)));

  cross_device_footer->AddChildView(CreateGooglePasswordManagerLabel(
      GetHelpTextMessageId(),
      /*link_message_id=*/
      IDS_PASSWORD_BUBBLES_PASSWORD_MANAGER_LINK_TEXT_SYNCED_TO_ACCOUNT,
      primary_account_email));

  return cross_device_footer;
}

// Displays the "edit password" row with custom logic for handling mouse events
// (background selection and switching to editing state on clicks).
class EditPasswordRow : public views::FlexLayoutView {
  METADATA_HEADER(EditPasswordRow, views::FlexLayoutView)

 public:
  explicit EditPasswordRow(
      base::WeakPtr<PasswordGenerationPopupController> controller)
      : controller_(controller) {
    auto icon = std::make_unique<NonAccessibleImageView>();
    icon->SetImage(ui::ImageModel::FromVectorIcon(
        vector_icons::kEditIcon, ui::kColorIconSecondary, kIconSize));
    AddChildView(std::move(icon));

    auto spacer = std::make_unique<views::View>();
    spacer->SetPreferredSize(gfx::Size(
        autofill::PopupBaseView::ArrowHorizontalMargin(), /*height=*/1));
    AddChildView(std::move(spacer));

    AddChildView(std::make_unique<views::Label>(
        l10n_util::GetStringUTF16(IDS_PASSWORD_GENERATION_EDIT_PASSWORD),
        views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_PRIMARY));
  }

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    if (!controller_) {
      return;
    }

    node_data->role = ax::mojom::Role::kListBoxOption;
    node_data->AddBoolAttribute(ax::mojom::BoolAttribute::kSelected,
                                controller_->edit_password_selected());
    node_data->SetNameChecked(
        l10n_util::GetStringUTF16(IDS_PASSWORD_GENERATION_EDIT_PASSWORD));
    const std::u16string help_text = l10n_util::GetStringFUTF16(
        GetHelpTextMessageId(),
        l10n_util::GetStringUTF16(
            IDS_PASSWORD_BUBBLES_PASSWORD_MANAGER_LINK_TEXT_SYNCED_TO_ACCOUNT),
        controller_->GetPrimaryAccountEmail());
    node_data->SetDescription(help_text);
  }

 private:
  void OnMouseEntered(const ui::MouseEvent& event) override {
    if (controller_) {
      controller_->EditPasswordHovered(true);
    }
  }

  void OnMouseExited(const ui::MouseEvent& event) override {
    if (controller_) {
      controller_->EditPasswordHovered(false);
    }
  }

  bool OnMousePressed(const ui::MouseEvent& event) override {
    return event.GetClickCount() == 1;
  }

  void OnMouseReleased(const ui::MouseEvent& event) override {
    if (event.IsOnlyLeftMouseButton() && controller_) {
      controller_->EditPasswordClicked();
    }
  }

  base::WeakPtr<PasswordGenerationPopupController> controller_ = nullptr;
};

BEGIN_METADATA(EditPasswordRow)
END_METADATA

// Creates row with Password Manager key icon and title for the
// `kPasswordGenerationExperiment` with `kNudgePassword` variation.
std::unique_ptr<views::View> CreatePasswordLabelWithIcon() {
  auto view = std::make_unique<views::View>();
  view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));

  view->AddChildView(
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          GooglePasswordManagerVectorIcon(), ui::kColorIcon, kIconSize)));

  AddSpacerWithSize(autofill::PopupBaseView::ArrowHorizontalMargin(),
                    /*resize=*/false, view.get());

  auto* label = view->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_PASSWORD_GENERATION_NUDGE_TITLE),
      views::style::CONTEXT_DIALOG_TITLE, views::style::STYLE_PRIMARY));
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetMultiLine(true);
  label->SetMaximumWidth(kPasswordGenerationMaxWidth);

  return view;
}

// Creates row with two buttons aligned to the right. First button is a cancel
// button that dismisses the generation flow and the second one is an accept
// button that fills the password suggestion and dismisses the popup.
class NudgePasswordButtons : public views::View {
  METADATA_HEADER(NudgePasswordButtons, views::View)

 public:
  explicit NudgePasswordButtons(
      base::WeakPtr<PasswordGenerationPopupController> controller)
      : controller_(controller) {
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal));
    layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kEnd);

    const std::u16string help_text = base::JoinString(
        {l10n_util::GetStringUTF16(IDS_PASSWORD_GENERATION_NUDGE_TITLE),
         l10n_util::GetStringFUTF16(
             GetHelpTextMessageId(),
             l10n_util::GetStringUTF16(
                 IDS_PASSWORD_BUBBLES_PASSWORD_MANAGER_LINK_TEXT_SYNCED_TO_ACCOUNT),
             controller_->GetPrimaryAccountEmail())},
        u" ");
    const std::u16string cancel_button_label =
        l10n_util::GetStringUTF16(IDS_PASSWORD_GENERATION_NUDGE_CANCEL_BUTTON);
    auto cancel_button = std::make_unique<views::MdTextButton>(
        base::BindRepeating(&NudgePasswordButtons::CancelButtonPressed,
                            base::Unretained(this)),
        cancel_button_label);
    cancel_button->GetViewAccessibility().SetRole(
        ax::mojom::Role::kListBoxOption);
    cancel_button->GetViewAccessibility().SetName(cancel_button_label);
    cancel_button->GetViewAccessibility().SetDescription(help_text);
    cancel_button_ = AddChildView(std::move(cancel_button));

    AddSpacerWithSize(autofill::PopupBaseView::ArrowHorizontalMargin(),
                      /*resize=*/false, this);

    const std::u16string accept_button_label =
        l10n_util::GetStringUTF16(IDS_PASSWORD_GENERATION_SUGGESTION_GPM);
    auto accept_button = std::make_unique<views::MdTextButton>(
        base::BindRepeating(&NudgePasswordButtons::AcceptButtonPressed,
                            base::Unretained(this)),
        accept_button_label);
    accept_button->SetStyle(ui::ButtonStyle::kProminent);
    accept_button->GetViewAccessibility().SetRole(
        ax::mojom::Role::kListBoxOption);
    accept_button->GetViewAccessibility().SetName(
        base::JoinString({accept_button_label, controller_->password()}, u" "));
    accept_button->GetViewAccessibility().SetDescription(help_text);
    accept_button_ = AddChildView(std::move(accept_button));

    // Set up custom focus predicates for buttons as the default ones check if
    // they actually have focus, which won't be happening since the parent
    // widget (of the popup view) is not activatable.
    views::FocusRing::Get(accept_button_)
        ->SetHasFocusPredicate(base::BindRepeating(
            [](const NudgePasswordButtons* buttons, const views::View* view) {
              return buttons->accept_button_has_focus_;
            },
            base::Unretained(this)));
    views::FocusRing::Get(cancel_button_)
        ->SetHasFocusPredicate(base::BindRepeating(
            [](const NudgePasswordButtons* buttons, const views::View* view) {
              return buttons->cancel_button_has_focus_;
            },
            base::Unretained(this)));
  }

  void UpdateFocus(bool accept_button_focused) {
    accept_button_has_focus_ = accept_button_focused;
    cancel_button_has_focus_ = !accept_button_focused;
    views::FocusRing::Get(accept_button_)->SchedulePaint();
    views::FocusRing::Get(cancel_button_)->SchedulePaint();
  }

  views::View* GetAcceptButton() { return accept_button_; }
  views::View* GetCancelButton() { return cancel_button_; }

 private:
  void CancelButtonPressed() {
    if (controller_) {
      controller_->Hide(autofill::SuggestionHidingReason::kUserAborted);
    }
  }
  void AcceptButtonPressed() {
    if (controller_) {
      controller_->PasswordAccepted();
    }
  }

  base::WeakPtr<PasswordGenerationPopupController> controller_ = nullptr;
  raw_ptr<views::View> accept_button_ = nullptr;
  raw_ptr<views::View> cancel_button_ = nullptr;
  bool accept_button_has_focus_ = false;
  bool cancel_button_has_focus_ = false;
};

BEGIN_METADATA(NudgePasswordButtons)
END_METADATA

// Creates custom password generation view with key icon, title and two buttons
// for `kNudgePassword` variant of `kPasswordGenerationExperiment`.
std::unique_ptr<views::View> CreateNudgePasswordView(
    base::WeakPtr<PasswordGenerationPopupController> controller) {
  auto nudge_password_view = std::make_unique<views::View>();

  auto* layout =
      nudge_password_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  layout->set_between_child_spacing(
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_UNRELATED_CONTROL_VERTICAL_LARGE));

  nudge_password_view->AddChildView(CreatePasswordLabelWithIcon());

  nudge_password_view->AddChildView(CreateGooglePasswordManagerLabel(
      GetHelpTextMessageId(),
      /*link_message_id=*/
      IDS_PASSWORD_BUBBLES_PASSWORD_MANAGER_LINK_TEXT_SYNCED_TO_ACCOUNT,
      controller->GetPrimaryAccountEmail()));

  return nudge_password_view;
}

}  // namespace

// Class that shows the generated password and associated UI (currently an
// explanatory text).
class PasswordGenerationPopupViewViews::GeneratedPasswordBox
    : public views::View {
  METADATA_HEADER(GeneratedPasswordBox, views::View)

 public:
  GeneratedPasswordBox() = default;
  ~GeneratedPasswordBox() override = default;

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    if (!controller_) {
      return;
    }

    node_data->role = ax::mojom::Role::kListBoxOption;
    node_data->AddBoolAttribute(ax::mojom::BoolAttribute::kSelected,
                                controller_->password_selected());
    node_data->SetNameChecked(base::JoinString(
        {controller_->SuggestedText(), controller_->password()}, u" "));
    const std::u16string help_text = l10n_util::GetStringFUTF16(
        GetHelpTextMessageId(),
        l10n_util::GetStringUTF16(
            IDS_PASSWORD_BUBBLES_PASSWORD_MANAGER_LINK_TEXT_SYNCED_TO_ACCOUNT),
        controller_->GetPrimaryAccountEmail());

    if (password_manager::features::kPasswordGenerationExperimentVariationParam
            .Get() == PasswordGenerationVariation::kCrossDevice) {
      const std::u16string description = base::JoinString(
          {l10n_util::GetStringUTF16(IDS_PASSWORD_GENERATION_BENEFITS),
           l10n_util::GetStringUTF16(IDS_PASSWORD_GENERATION_CROSS_DEVICE),
           l10n_util::GetStringUTF16(IDS_PASSWORD_GENERATION_SECURITY),
           l10n_util::GetStringUTF16(IDS_PASSWORD_GENERATION_PROACTIVE_CHECK),
           help_text},
          u" ");
      node_data->SetDescription(description);
      return;
    }

    node_data->SetDescription(help_text);
  }

  // Fills the view with strings provided by |controller|.
  void Init(base::WeakPtr<PasswordGenerationPopupController> controller);

  void UpdateGeneratedPassword(const std::u16string& password) {
    password_label_->SetText(password);
  }

  void UpdateBackground(ui::ColorId color) {
    SetBackground(views::CreateThemedSolidBackground(color));
    // Setting a background color on the labels may change the text color to
    // improve contrast.
    password_label_->SetBackgroundColorId(color);
    suggestion_label_->SetBackgroundColorId(color);
  }

  void reset_controller() { controller_ = nullptr; }

 private:
  // Implements the View interface.
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  raw_ptr<views::Label> suggestion_label_ = nullptr;
  raw_ptr<views::Label> password_label_ = nullptr;
  base::WeakPtr<PasswordGenerationPopupController> controller_ = nullptr;
};

void PasswordGenerationPopupViewViews::GeneratedPasswordBox::Init(
    base::WeakPtr<PasswordGenerationPopupController> controller) {
  // Make sure we only receive enter/exit events when the mouse enters the whole
  // view, even if it is entering/exiting a child view. This is needed to
  // prevent the background highlight of the password box disappearing when
  // entering the key icon view (see crbug.com/1393991).
  SetNotifyEnterExitOnChild(true);

  controller_ = controller;
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  AddChildView(
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          GooglePasswordManagerVectorIcon(), ui::kColorIcon, kIconSize)));
  AddSpacerWithSize(PopupBaseView::ArrowHorizontalMargin(),
                    /*resize=*/false, this);

  suggestion_label_ = AddChildView(std::make_unique<views::Label>(
      controller_->SuggestedText(), views::style::CONTEXT_DIALOG_BODY_TEXT,
      controller_->state() ==
              PasswordGenerationPopupController::kOfferGeneration
          ? views::style::STYLE_PRIMARY
          : views::style::STYLE_SECONDARY));

  AddSpacerWithSize(
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_BETWEEN_PRIMARY_AND_SECONDARY_LABELS_HORIZONTAL),
      /*resize=*/true, this);

  DCHECK(!password_label_);
  password_label_ = AddChildView(std::make_unique<views::Label>(
      controller_->password(), views::style::CONTEXT_DIALOG_BODY_TEXT,
      STYLE_SECONDARY_MONOSPACED));
  layout->SetFlexForView(password_label_, 1);
}

void PasswordGenerationPopupViewViews::GeneratedPasswordBox::OnMouseEntered(
    const ui::MouseEvent& event) {
  if (controller_) {
    controller_->SetSelected();
  }
}

void PasswordGenerationPopupViewViews::GeneratedPasswordBox::OnMouseExited(
    const ui::MouseEvent& event) {
  if (controller_) {
    controller_->SelectionCleared();
  }
}

bool PasswordGenerationPopupViewViews::GeneratedPasswordBox::OnMousePressed(
    const ui::MouseEvent& event) {
  return event.GetClickCount() == 1;
}

void PasswordGenerationPopupViewViews::GeneratedPasswordBox::OnMouseReleased(
    const ui::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton() && controller_) {
    controller_->PasswordAccepted();
  }
}

void PasswordGenerationPopupViewViews::GeneratedPasswordBox::OnGestureEvent(
    ui::GestureEvent* event) {
  if (!controller_) {
    return;
  }
  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN:
      controller_->SetSelected();
      break;
    case ui::ET_GESTURE_TAP:
      controller_->PasswordAccepted();
      break;
    case ui::ET_GESTURE_TAP_CANCEL:
    case ui::ET_GESTURE_END:
      controller_->SelectionCleared();
      break;
    default:
      return;
  }
}

BEGIN_METADATA(PasswordGenerationPopupViewViews, GeneratedPasswordBox)
END_METADATA

PasswordGenerationPopupViewViews::PasswordGenerationPopupViewViews(
    base::WeakPtr<PasswordGenerationPopupController> controller,
    views::Widget* parent_widget)
    : PopupBaseView(controller, parent_widget), controller_(controller) {
  CreateLayoutAndChildren();
}

PasswordGenerationPopupViewViews::~PasswordGenerationPopupViewViews() = default;

bool PasswordGenerationPopupViewViews::Show() {
  return DoShow();
}

void PasswordGenerationPopupViewViews::Hide() {
  // The controller is no longer valid after it hides us.
  controller_ = nullptr;
  if (password_view_) {
    password_view_->reset_controller();
  }

  DoHide();
}

void PasswordGenerationPopupViewViews::UpdateState() {
  password_view_ = nullptr;
  edit_password_view_ = nullptr;
  nudge_password_buttons_view_ = nullptr;
  RemoveAllChildViews();
  CreateLayoutAndChildren();
}

void PasswordGenerationPopupViewViews::UpdateGeneratedPasswordValue() {
  if (password_view_) {
    password_view_->UpdateGeneratedPassword(controller_->password());
  }
  DeprecatedLayoutImmediately();
}

bool PasswordGenerationPopupViewViews::UpdateBoundsAndRedrawPopup() {
  return DoUpdateBoundsAndRedrawPopup();
}

void PasswordGenerationPopupViewViews::PasswordSelectionUpdated() {
  if (!GetWidget() || !password_view_) {
    return;
  }

  CHECK(password_view_);
  if (controller_->password_selected()) {
    NotifyAXSelection(*this->password_view_);
  }

  password_view_->UpdateBackground(controller_->password_selected()
                                       ? ui::kColorDropdownBackgroundSelected
                                       : ui::kColorDropdownBackground);
  SchedulePaint();
}

void PasswordGenerationPopupViewViews::EditPasswordSelectionUpdated() {
  if (!GetWidget() || !edit_password_view_) {
    return;
  }

  if (controller_->edit_password_selected()) {
    CHECK(this->edit_password_view_);
    NotifyAXSelection(*this->edit_password_view_);
  }

  edit_password_view_->SetBackground(views::CreateThemedSolidBackground(
      controller_->edit_password_selected()
          ? ui::kColorDropdownBackgroundSelected
          : ui::kColorDropdownBackground));
  SchedulePaint();
}

void PasswordGenerationPopupViewViews::NudgePasswordSelectionUpdated() {
  if (!GetWidget() || !nudge_password_buttons_view_) {
    return;
  }

  auto* nudge_password_buttons =
      static_cast<NudgePasswordButtons*>(nudge_password_buttons_view_);
  if (controller_->accept_button_selected()) {
    NotifyAXSelection(*nudge_password_buttons->GetAcceptButton());
  } else if (controller_->cancel_button_selected()) {
    NotifyAXSelection(*nudge_password_buttons->GetCancelButton());
  }
  nudge_password_buttons->UpdateFocus(controller_->accept_button_selected());
}

void PasswordGenerationPopupViewViews::CreateLayoutAndChildren() {
  SetBackground(
      views::CreateThemedSolidBackground(ui::kColorDropdownBackground));

  views::BoxLayout* box_layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  const ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  const int kVerticalPadding =
      provider->GetDistanceMetric(DISTANCE_TOAST_LABEL_VERTICAL);
  const int kHorizontalMargin =
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_HORIZONTAL);

  if (controller_->ShouldShowNudgePassword()) {
    auto nudge_password_view = CreateNudgePasswordView(controller_);
    nudge_password_buttons_view_ = nudge_password_view->AddChildView(
        std::make_unique<NudgePasswordButtons>(controller_));
    nudge_password_view->SetBorder(views::CreateEmptyBorder(
        gfx::Insets::VH(kVerticalPadding, kHorizontalMargin)));
    AddChildView(std::move(nudge_password_view));
    return;
  }

  auto password_view = std::make_unique<GeneratedPasswordBox>();
  password_view->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::VH(kVerticalPadding, kHorizontalMargin)));
  password_view->Init(controller_);
  password_view_ = AddChildView(std::move(password_view));
  PasswordSelectionUpdated();

  if (controller_->state() ==
          PasswordGenerationPopupController::kOfferGeneration &&
      password_manager::features::kPasswordGenerationExperimentVariationParam
              .Get() == PasswordGenerationVariation::kEditPassword) {
    AddChildView(views::Builder<views::Separator>()
                     .SetOrientation(views::Separator::Orientation::kHorizontal)
                     .SetColorId(ui::kColorMenuSeparator)
                     .Build());

    auto edit_password_row = std::make_unique<EditPasswordRow>(controller_);
    edit_password_row->SetBorder(views::CreateEmptyBorder(
        gfx::Insets::VH(kVerticalPadding, kHorizontalMargin)));
    edit_password_view_ = AddChildView(std::move(edit_password_row));
  }

  AddChildView(views::Builder<views::Separator>()
                   .SetOrientation(views::Separator::Orientation::kHorizontal)
                   .SetColorId(ui::kColorMenuSeparator)
                   .Build());

  if (password_manager::features::kPasswordGenerationExperimentVariationParam
          .Get() == PasswordGenerationVariation::kCrossDevice) {
    auto* cross_device_footer = AddChildView(
        CreateCrossDeviceFooter(controller_->GetPrimaryAccountEmail()));
    cross_device_footer->SetBorder(views::CreateEmptyBorder(
        gfx::Insets::VH(kVerticalPadding, kHorizontalMargin)));
    return;
  }

  views::Label* help_label = AddChildView(CreateGooglePasswordManagerLabel(
      GetHelpTextMessageId(),
      /*link_message_id=*/
      IDS_PASSWORD_BUBBLES_PASSWORD_MANAGER_LINK_TEXT_SYNCED_TO_ACCOUNT,
      controller_->GetPrimaryAccountEmail()));

  help_label->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::VH(kVerticalPadding, kHorizontalMargin)));
}

void PasswordGenerationPopupViewViews::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  // TODO(crbug.com/40885943): kListBox is used for the same reason as in
  // `autofill::PopupViewViews`. See crrev.com/c/2545285 for details.
  // Consider using a more appropriate role (e.g. kMenuListPopup or similar).
  node_data->role = ax::mojom::Role::kListBox;

  if (!controller_) {
    node_data->AddState(ax::mojom::State::kCollapsed);
    node_data->AddState(ax::mojom::State::kInvisible);
    return;
  }

  node_data->AddState(ax::mojom::State::kExpanded);
}

gfx::Size PasswordGenerationPopupViewViews::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  if (controller_->ShouldShowNudgePassword()) {
    int width =
        std::min(views::View::CalculatePreferredSize(available_size).width(),
                 kPasswordGenerationMaxWidth);
    return gfx::Size(
        width, GetLayoutManager()->GetPreferredHeightForWidth(this, width));
  }

  int width =
      std::max(password_view_->GetPreferredSize().width(),
               gfx::ToEnclosingRect(controller_->element_bounds()).width());
  width = std::min(width, kPasswordGenerationMaxWidth);
  return gfx::Size(width,
                   GetLayoutManager()->GetPreferredHeightForWidth(this, width));
}

PasswordGenerationPopupView* PasswordGenerationPopupView::Create(
    base::WeakPtr<PasswordGenerationPopupController> controller) {
  if (!controller->container_view()) {
    return nullptr;
  }

  views::Widget* observing_widget =
      views::Widget::GetTopLevelWidgetForNativeView(
          controller->container_view());

  return new PasswordGenerationPopupViewViews(controller, observing_widget);
}

BEGIN_METADATA(PasswordGenerationPopupViewViews)
END_METADATA
