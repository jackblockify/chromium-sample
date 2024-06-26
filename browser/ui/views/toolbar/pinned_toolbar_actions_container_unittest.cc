// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"

#include <vector>

#include "base/functional/bind.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model_factory.h"
#include "chrome/browser/ui/toolbar/toolbar_pref_names.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_context.h"
#include "ui/actions/action_id.h"
#include "ui/actions/actions.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"

class PinnedToolbarActionsContainerTest : public TestWithBrowserView {
 public:
  void SetUp() override {
    TestWithBrowserView::SetUp();
    AddTab(browser_view()->browser(), GURL("http://foo1.com"));
    browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);

    model_ = PinnedToolbarActionsModel::Get(profile());
    ASSERT_TRUE(model_);
  }

  void TearDown() override {
    model_ = nullptr;
    TestWithBrowserView::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    TestingProfile::TestingFactories factories =
        TestWithBrowserView::GetTestingFactories();
    factories.emplace_back(
        PinnedToolbarActionsModelFactory::GetInstance(),
        base::BindRepeating(&PinnedToolbarActionsContainerTest::
                                BuildPinnedToolbarActionsModel));
    return factories;
  }

  static std::unique_ptr<KeyedService> BuildPinnedToolbarActionsModel(
      content::BrowserContext* context) {
    return std::make_unique<PinnedToolbarActionsModel>(
        Profile::FromBrowserContext(context));
  }

  std::vector<PinnedActionToolbarButton*> GetChildToolbarButtons() {
    std::vector<PinnedActionToolbarButton*> result;
    for (views::View* child : container()->children()) {
      if (views::Button::AsButton(child)) {
        PinnedActionToolbarButton* button =
            static_cast<PinnedActionToolbarButton*>(child);
#if BUILDFLAG(IS_MAC)
        // TODO(crbug.com/40670141): Query the model on whether this button
        // should be visible since buttons in the toolbar are only removed after
        // animations finish running, which is not reliable in unit tests on
        // Mac.
        if (!container()->IsActionPinnedOrPoppedOut(button->GetActionId())) {
          continue;
        }
#endif
        result.push_back(button);
      }
    }
    return result;
  }

  void CheckIsPoppedOut(actions::ActionId id, bool should_be_popped_out) {
    auto* container =
        browser_view()->toolbar()->pinned_toolbar_actions_container();
    if (should_be_popped_out) {
      ASSERT_NE(base::ranges::find(container->popped_out_buttons_, id,
                                   [](PinnedActionToolbarButton* button) {
                                     return button->GetActionId();
                                   }),
                container->popped_out_buttons_.end());
    } else {
      ASSERT_EQ(base::ranges::find(container->popped_out_buttons_, id,
                                   [](PinnedActionToolbarButton* button) {
                                     return button->GetActionId();
                                   }),
                container->popped_out_buttons_.end());
    }
  }

  void CheckIsPinned(actions::ActionId id, bool should_be_pinned) {
    auto* container =
        browser_view()->toolbar()->pinned_toolbar_actions_container();
    if (should_be_pinned) {
      ASSERT_NE(base::ranges::find(container->pinned_buttons_, id,
                                   [](PinnedActionToolbarButton* button) {
                                     return button->GetActionId();
                                   }),
                container->pinned_buttons_.end());
    } else {
      ASSERT_EQ(base::ranges::find(container->pinned_buttons_, id,
                                   [](PinnedActionToolbarButton* button) {
                                     return button->GetActionId();
                                   }),
                container->pinned_buttons_.end());
    }
  }

  std::unique_ptr<actions::ActionItem> CreateActionItem(
      const actions::ActionId& id) {
    return actions::ActionItem::Builder()
        .SetText(u"Test Action")
        .SetTooltipText(u"Test Action")
        .SetActionId(id)
        .SetImage(ui::ImageModel::FromVectorIcon(vector_icons::kDogfoodIcon))
        .SetVisible(true)
        .SetEnabled(true)
        .SetInvokeActionCallback(base::DoNothing())
        .Build();
  }

  void UpdatePref(const std::vector<actions::ActionId>& updated_list) {
    ScopedListPrefUpdate update(profile()->GetPrefs(), prefs::kPinnedActions);
    base::Value::List& list_of_values = update.Get();
    list_of_values.clear();
    for (auto id : updated_list) {
      const std::optional<std::string>& id_string =
          actions::ActionIdMap::ActionIdToString(id);
      // The ActionId should have a string equivalent.
      CHECK(id_string.has_value());
      list_of_values.Append(id_string.value());
    }
  }

  void WaitForAnimations() {
#if BUILDFLAG(IS_MAC)
    // TODO(crbug.com/40670141): we avoid using animations on Mac due to the
    // lack of support in unit tests. Therefore this is a no-op.
#else
    views::test::WaitForAnimatingLayoutManager(container());
#endif
  }

  PinnedToolbarActionsContainer* container() {
    return browser_view()->toolbar()->pinned_toolbar_actions_container();
  }

  PinnedToolbarActionsModel* model() { return model_.get(); }

  void SendKeyPress(views::View* view,
                    ui::KeyboardCode code,
                    int flags = ui::EF_NONE) {
    view->OnKeyPressed(
        ui::KeyEvent(ui::ET_KEY_PRESSED, code, flags, ui::EventTimeForNow()));
    view->OnKeyReleased(
        ui::KeyEvent(ui::ET_KEY_PRESSED, code, flags, ui::EventTimeForNow()));
  }

 private:
  raw_ptr<PinnedToolbarActionsModel> model_;
};

TEST_F(PinnedToolbarActionsContainerTest, ContainerMargins) {
  // Verify margins are correctly set.
  ASSERT_EQ(container()
                ->GetTargetLayoutManager()
                ->GetDefaultForTesting(views::kMarginsKey)
                ->left(),
            GetLayoutConstant(TOOLBAR_ICON_DEFAULT_MARGIN));
  ASSERT_EQ(container()
                ->GetTargetLayoutManager()
                ->GetDefaultForTesting(views::kMarginsKey)
                ->right(),
            GetLayoutConstant(TOOLBAR_ICON_DEFAULT_MARGIN));
}

TEST_F(PinnedToolbarActionsContainerTest, PinningAndUnpinning) {
  actions::ActionItem* browser_action_item =
      browser_view()->browser()->browser_actions()->root_action_item();

  // clang-format on
  browser_action_item->AddChild(CreateActionItem(actions::kActionCut));

  // Verify there are no pinned buttons.
  auto pinned_buttons = GetChildToolbarButtons();
  ASSERT_EQ(pinned_buttons.size(), 0u);
  // Verify pinning an action adds a button.
  model()->UpdatePinnedState(actions::kActionCut, true);
  pinned_buttons = GetChildToolbarButtons();
  ASSERT_EQ(pinned_buttons.size(), 1u);
  // Check the context menu
  EXPECT_EQ(
      pinned_buttons[0]->menu_model()->GetLabelAt(0),
      l10n_util::GetStringUTF16(IDS_SIDE_PANEL_TOOLBAR_BUTTON_CXMENU_UNPIN));
  // Verify pressing the toolbar button invokes the action.
  ASSERT_EQ(actions::ActionManager::Get()
                .FindAction(actions::kActionCut)
                ->GetInvokeCount(),
            0);
  pinned_buttons[0]->button_controller()->NotifyClick();
  ASSERT_EQ(actions::ActionManager::Get()
                .FindAction(actions::kActionCut)
                ->GetInvokeCount(),
            1);
  // Verify unpinning an action removes a button.
  model()->UpdatePinnedState(actions::kActionCut, false);
  WaitForAnimations();
  pinned_buttons = GetChildToolbarButtons();
  ASSERT_EQ(pinned_buttons.size(), 0u);
}

TEST_F(PinnedToolbarActionsContainerTest,
       UnpinnedToolbarButtonsPoppedOutWhileActive) {
  actions::ActionItem* browser_action_item =
      browser_view()->browser()->browser_actions()->root_action_item();

  browser_action_item->AddChild(CreateActionItem(actions::kActionCut));

  // Verify there are no pinned buttons.
  auto toolbar_buttons = GetChildToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 0u);
  // Verify activating a button does not pin and adds to popped out buttons.
  container()->UpdateActionState(actions::kActionCut, true);
  CheckIsPoppedOut(actions::kActionCut, true);
  CheckIsPinned(actions::kActionCut, false);
  toolbar_buttons = GetChildToolbarButtons();
  // Check the context menu
  EXPECT_EQ(
      toolbar_buttons[0]->menu_model()->GetLabelAt(0),
      l10n_util::GetStringUTF16(IDS_SIDE_PANEL_TOOLBAR_BUTTON_CXMENU_PIN));
  ASSERT_EQ(toolbar_buttons.size(), 1u);
  // Verify deactivating a button removes it from popped out buttons.
  container()->UpdateActionState(actions::kActionCut, false);
  WaitForAnimations();
  CheckIsPoppedOut(actions::kActionCut, false);
  CheckIsPinned(actions::kActionCut, false);
  toolbar_buttons = GetChildToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 0u);
}

TEST_F(PinnedToolbarActionsContainerTest,
       StateChangesBetweenPinnedandUnpinnedWhileActive) {
  actions::ActionItem* browser_action_item =
      browser_view()->browser()->browser_actions()->root_action_item();

  browser_action_item->AddChild(CreateActionItem(actions::kActionCut));

  // Verify there are no pinned buttons.
  auto toolbar_buttons = GetChildToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 0u);
  // Verify activating a button does not pin and adds to popped out buttons.
  container()->UpdateActionState(actions::kActionCut, true);
  CheckIsPoppedOut(actions::kActionCut, true);
  CheckIsPinned(actions::kActionCut, false);
  toolbar_buttons = GetChildToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 1u);
  // Pin active button and verify state.
  model()->UpdatePinnedState(actions::kActionCut, true);
  CheckIsPoppedOut(actions::kActionCut, false);
  CheckIsPinned(actions::kActionCut, true);
  toolbar_buttons = GetChildToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 1u);
  // Unpin active button and verify state.
  model()->UpdatePinnedState(actions::kActionCut, false);
  WaitForAnimations();
  CheckIsPoppedOut(actions::kActionCut, true);
  CheckIsPinned(actions::kActionCut, false);
  toolbar_buttons = GetChildToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 1u);
}

TEST_F(PinnedToolbarActionsContainerTest, PoppedOutButtonsAreAfterPinned) {
  actions::ActionItem* browser_action_item =
      browser_view()->browser()->browser_actions()->root_action_item();

  browser_action_item->AddChild(CreateActionItem(actions::kActionCut));
  browser_action_item->AddChild(CreateActionItem(actions::kActionCopy));

  // Verify there are no pinned buttons.
  auto toolbar_buttons = GetChildToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 0u);
  // Pin both and verify order matches the order they were added.
  model()->UpdatePinnedState(actions::kActionCut, true);
  model()->UpdatePinnedState(actions::kActionCopy, true);
  toolbar_buttons = GetChildToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 2u);
  ASSERT_EQ(toolbar_buttons[0]->GetActionId(), actions::kActionCut);
  ASSERT_EQ(toolbar_buttons[1]->GetActionId(), actions::kActionCopy);
  // Make kActionCut popped out instead of pinned and verify order.
  container()->UpdateActionState(actions::kActionCut, true);
  model()->UpdatePinnedState(actions::kActionCut, false);
  WaitForAnimations();
  CheckIsPoppedOut(actions::kActionCut, true);
  CheckIsPinned(actions::kActionCut, false);
  toolbar_buttons = GetChildToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 2u);
  ASSERT_EQ(toolbar_buttons[0]->GetActionId(), actions::kActionCopy);
  ASSERT_EQ(toolbar_buttons[1]->GetActionId(), actions::kActionCut);
}

TEST_F(PinnedToolbarActionsContainerTest, DividerVisibleWhileButtonPoppedOut) {
  actions::ActionItem* browser_action_item =
      browser_view()->browser()->browser_actions()->root_action_item();

  browser_action_item->AddChild(CreateActionItem(actions::kActionCut));

  // Verify there are no child views visible. Note the divider still exists but
  // should not be visible.
  auto child_views = container()->children();
  ASSERT_EQ(child_views.size(), 1u);
  ASSERT_FALSE(child_views[0]->GetVisible());
  // Make kActionCut popped out and verify order and visibility of the divider.
  container()->UpdateActionState(actions::kActionCut, true);
  CheckIsPoppedOut(actions::kActionCut, true);
  CheckIsPinned(actions::kActionCut, false);
  child_views = container()->children();
  ASSERT_EQ(child_views.size(), 2u);
  ASSERT_EQ(
      static_cast<PinnedActionToolbarButton*>(child_views[0])->GetActionId(),
      actions::kActionCut);
  ASSERT_EQ(child_views[1]->GetProperty(views::kElementIdentifierKey),
            kPinnedToolbarActionsContainerDividerElementId);
  ASSERT_FALSE(child_views[1]->GetVisible());
  // Pin kActionCut and verify the pinned button is there and the divider is
  // visible.
  model()->UpdatePinnedState(actions::kActionCut, true);
  CheckIsPoppedOut(actions::kActionCut, false);
  CheckIsPinned(actions::kActionCut, true);
  child_views = container()->children();
  ASSERT_EQ(child_views.size(), 2u);
  ASSERT_EQ(
      static_cast<PinnedActionToolbarButton*>(child_views[0])->GetActionId(),
      actions::kActionCut);
  ASSERT_EQ(child_views[1]->GetProperty(views::kElementIdentifierKey),
            kPinnedToolbarActionsContainerDividerElementId);
  ASSERT_TRUE(child_views[1]->GetVisible());
}

TEST_F(PinnedToolbarActionsContainerTest, MovingActionsUpdateOrderUsingDrag) {
  actions::ActionItem* browser_action_item =
      browser_view()->browser()->browser_actions()->root_action_item();

  browser_action_item->AddChild(CreateActionItem(actions::kActionCut));
  browser_action_item->AddChild(CreateActionItem(actions::kActionCopy));

  // Verify there are no pinned buttons.
  auto toolbar_buttons = GetChildToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 0u);
  // Pin both and verify order matches the order they were added.
  model()->UpdatePinnedState(actions::kActionCut, true);
  model()->UpdatePinnedState(actions::kActionCopy, true);
  toolbar_buttons = GetChildToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 2u);
  ASSERT_EQ(toolbar_buttons[0]->GetActionId(), actions::kActionCut);
  ASSERT_EQ(toolbar_buttons[1]->GetActionId(), actions::kActionCopy);
  // Drag to reorder the two actions.
  auto* drag_view = toolbar_buttons[1];
  EXPECT_TRUE(
      container()->CanStartDragForView(drag_view, gfx::Point(), gfx::Point()));
  ui::OSExchangeData drag_data;
  container()->WriteDragDataForView(drag_view, gfx::Point(), &drag_data);
  gfx::Point drag_location = toolbar_buttons[0]->bounds().CenterPoint();
  ui::DropTargetEvent drop_event(drag_data, gfx::PointF(drag_location),
                                 gfx::PointF(drag_location),
                                 ui::DragDropTypes::DRAG_MOVE);
  container()->OnDragUpdated(drop_event);
  auto drop_cb = container()->GetDropCallback(drop_event);
  ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
  std::move(drop_cb).Run(drop_event, output_drag_op,
                         /*drag_image_layer_owner=*/nullptr);
  WaitForAnimations();

  // Verify the order gets updated in the ui.
  toolbar_buttons = GetChildToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 2u);
  ASSERT_EQ(toolbar_buttons[0]->GetActionId(), actions::kActionCopy);
  ASSERT_EQ(toolbar_buttons[1]->GetActionId(), actions::kActionCut);
}

TEST_F(PinnedToolbarActionsContainerTest, ContextMenuTest) {
  actions::ActionItem* browser_action_item =
      browser_view()->browser()->browser_actions()->root_action_item();

  // clang-format on
  browser_action_item->AddChild(CreateActionItem(actions::kActionCut));

  // Verify there are no pinned buttons.
  auto pinned_buttons = GetChildToolbarButtons();
  ASSERT_EQ(pinned_buttons.size(), 0u);
  // Verify pinning an action adds a button.
  model()->UpdatePinnedState(actions::kActionCut, true);
  pinned_buttons = GetChildToolbarButtons();
  ASSERT_EQ(pinned_buttons.size(), 1u);
  // Check the context menu. Callback should unpin the button.
  EXPECT_EQ(
      pinned_buttons[0]->menu_model()->GetLabelAt(0),
      l10n_util::GetStringUTF16(IDS_SIDE_PANEL_TOOLBAR_BUTTON_CXMENU_UNPIN));
  pinned_buttons[0]->ExecuteCommand(IDC_UPDATE_SIDE_PANEL_PIN_STATE, 0);
  WaitForAnimations();
  pinned_buttons = GetChildToolbarButtons();
  ASSERT_EQ(pinned_buttons.size(), 0u);
  // Callback for pop out button should pin the action.
  container()->UpdateActionState(actions::kActionCut, true);
  auto child_views = container()->children();
  auto* pop_out_button =
      static_cast<PinnedActionToolbarButton*>(child_views[0]);
  EXPECT_EQ(
      pop_out_button->menu_model()->GetLabelAt(0),
      l10n_util::GetStringUTF16(IDS_SIDE_PANEL_TOOLBAR_BUTTON_CXMENU_PIN));
  pop_out_button->ExecuteCommand(IDC_UPDATE_SIDE_PANEL_PIN_STATE, 0);
  CheckIsPinned(actions::kActionCut, true);
}

TEST_F(PinnedToolbarActionsContainerTest, UpdatesFromSyncUpdateContainer) {
  actions::ActionItem* browser_action_item =
      browser_view()->browser()->browser_actions()->root_action_item();

  browser_action_item->AddChild(CreateActionItem(actions::kActionCut));
  browser_action_item->AddChild(CreateActionItem(actions::kActionCopy));
  browser_action_item->AddChild(CreateActionItem(actions::kActionPaste));

  // Verify there are no pinned buttons.
  auto toolbar_buttons = GetChildToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 0u);

  // Simulate an update where 2 actions are added to the prefs object.
  UpdatePref({actions::kActionCut, actions::kActionCopy});
  toolbar_buttons = GetChildToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 2u);
  ASSERT_EQ(toolbar_buttons[0]->GetActionId(), actions::kActionCut);
  ASSERT_EQ(toolbar_buttons[1]->GetActionId(), actions::kActionCopy);

  // Simulate an update where an action is added in between pinned actions.
  UpdatePref(
      {actions::kActionCut, actions::kActionPaste, actions::kActionCopy});
  toolbar_buttons = GetChildToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 3u);
  ASSERT_EQ(toolbar_buttons[0]->GetActionId(), actions::kActionCut);
  ASSERT_EQ(toolbar_buttons[1]->GetActionId(), actions::kActionPaste);
  ASSERT_EQ(toolbar_buttons[2]->GetActionId(), actions::kActionCopy);

  // Simulate an update where an action is removed from the pinned actions list.
  UpdatePref({actions::kActionPaste, actions::kActionCopy});
  WaitForAnimations();
  toolbar_buttons = GetChildToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 2u);
  ASSERT_EQ(toolbar_buttons[0]->GetActionId(), actions::kActionPaste);
  ASSERT_EQ(toolbar_buttons[1]->GetActionId(), actions::kActionCopy);

  // Simulate an update where an action is moved in the pinned actions list.
  UpdatePref({actions::kActionCopy, actions::kActionPaste});
  WaitForAnimations();
  toolbar_buttons = GetChildToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 2u);
  ASSERT_EQ(toolbar_buttons[0]->GetActionId(), actions::kActionCopy);
  ASSERT_EQ(toolbar_buttons[1]->GetActionId(), actions::kActionPaste);
}

TEST_F(PinnedToolbarActionsContainerTest,
       MovingActionsUpdateOrderUsingKeyboard) {
  actions::ActionItem* browser_action_item =
      browser_view()->browser()->browser_actions()->root_action_item();
  auto cut_action =
      actions::ActionItem::Builder()
          .SetText(u"Test Action")
          .SetTooltipText(u"Test Action")
          .SetActionId(actions::kActionCut)
          .SetImage(ui::ImageModel::FromVectorIcon(vector_icons::kDogfoodIcon))
          .SetVisible(true)
          .SetEnabled(true)
          .SetInvokeActionCallback(base::DoNothing())
          .Build();
  auto copy_action =
      actions::ActionItem::Builder()
          .SetText(u"Test Action")
          .SetTooltipText(u"Test Action")
          .SetActionId(actions::kActionCopy)
          .SetImage(ui::ImageModel::FromVectorIcon(vector_icons::kDogfoodIcon))
          .SetVisible(true)
          .SetEnabled(true)
          .SetInvokeActionCallback(base::DoNothing())
          .Build();
  auto paste_action =
      actions::ActionItem::Builder()
          .SetText(u"Test Action")
          .SetTooltipText(u"Test Action")
          .SetActionId(actions::kActionPaste)
          .SetImage(ui::ImageModel::FromVectorIcon(vector_icons::kDogfoodIcon))
          .SetVisible(true)
          .SetEnabled(true)
          .SetInvokeActionCallback(base::DoNothing())
          .Build();

  browser_action_item->AddChild(std::move(cut_action));
  browser_action_item->AddChild(std::move(copy_action));
  browser_action_item->AddChild(std::move(paste_action));

  auto* model = PinnedToolbarActionsModel::Get(profile());
  ASSERT_TRUE(model);
  // Verify there are no pinned buttons.
  auto toolbar_buttons = GetChildToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 0u);
  // Pin both and verify order matches the order they were added.
  model->UpdatePinnedState(actions::kActionCut, true);
  model->UpdatePinnedState(actions::kActionCopy, true);
  model->UpdatePinnedState(actions::kActionPaste, true);
  toolbar_buttons = GetChildToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 3u);
  ASSERT_EQ(toolbar_buttons[0]->GetActionId(), actions::kActionCut);
  ASSERT_EQ(toolbar_buttons[1]->GetActionId(), actions::kActionCopy);
  ASSERT_EQ(toolbar_buttons[2]->GetActionId(), actions::kActionPaste);

  constexpr int kModifiedFlag =
#if BUILDFLAG(IS_MAC)
      ui::EF_COMMAND_DOWN;
#else
      ui::EF_CONTROL_DOWN;
#endif

  // Reorder the first actions to the right using keyboard.
  SendKeyPress(toolbar_buttons[0], ui::VKEY_RIGHT, kModifiedFlag);
  toolbar_buttons = GetChildToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 3u);
  ASSERT_EQ(toolbar_buttons[0]->GetActionId(), actions::kActionCopy);
  ASSERT_EQ(toolbar_buttons[1]->GetActionId(), actions::kActionCut);
  ASSERT_EQ(toolbar_buttons[2]->GetActionId(), actions::kActionPaste);

  // Reorder the second actions to the right using keyboard.
  SendKeyPress(toolbar_buttons[1], ui::VKEY_RIGHT, kModifiedFlag);
  toolbar_buttons = GetChildToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 3u);
  ASSERT_EQ(toolbar_buttons[0]->GetActionId(), actions::kActionCopy);
  ASSERT_EQ(toolbar_buttons[1]->GetActionId(), actions::kActionPaste);
  ASSERT_EQ(toolbar_buttons[2]->GetActionId(), actions::kActionCut);

  // Reorder the last actions to the right using keyboard.
  SendKeyPress(toolbar_buttons[2], ui::VKEY_RIGHT, kModifiedFlag);
  toolbar_buttons = GetChildToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 3u);
  ASSERT_EQ(toolbar_buttons[0]->GetActionId(), actions::kActionCopy);
  ASSERT_EQ(toolbar_buttons[1]->GetActionId(), actions::kActionPaste);
  ASSERT_EQ(toolbar_buttons[2]->GetActionId(), actions::kActionCut);

  // Reorder the last actions to the left using keyboard.
  SendKeyPress(toolbar_buttons[2], ui::VKEY_LEFT, kModifiedFlag);
  toolbar_buttons = GetChildToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 3u);
  ASSERT_EQ(toolbar_buttons[0]->GetActionId(), actions::kActionCopy);
  ASSERT_EQ(toolbar_buttons[1]->GetActionId(), actions::kActionCut);
  ASSERT_EQ(toolbar_buttons[2]->GetActionId(), actions::kActionPaste);
}

TEST_F(PinnedToolbarActionsContainerTest,
       ActionRemainsInToolbarWhenSetToBeEphemerallyVisible) {
  actions::ActionItem* browser_action_item =
      browser_view()->browser()->browser_actions()->root_action_item();
  browser_action_item->AddChild(CreateActionItem(actions::kActionCut));

  // Verify there are no buttons.
  auto toolbar_buttons = GetChildToolbarButtons();
  ASSERT_EQ(toolbar_buttons.size(), 0u);
  // Verify setting as ephemerally visible pops out the button.
  container()->ShowActionEphemerallyInToolbar(actions::kActionCut, true);
  CheckIsPoppedOut(actions::kActionCut, true);
  CheckIsPinned(actions::kActionCut, false);
  // Verify pinning the button switches it to pinned.
  model()->UpdatePinnedState(actions::kActionCut, true);
  CheckIsPoppedOut(actions::kActionCut, false);
  CheckIsPinned(actions::kActionCut, true);
  // Verify it is still pinned when it does not need to be ephemerally shown.
  container()->ShowActionEphemerallyInToolbar(actions::kActionCut, false);
  CheckIsPoppedOut(actions::kActionCut, false);
  CheckIsPinned(actions::kActionCut, true);
  // Set as ephemerally visible again and verify it is still popped out when
  // unpinned.
  container()->ShowActionEphemerallyInToolbar(actions::kActionCut, true);
  model()->UpdatePinnedState(actions::kActionCut, false);
  CheckIsPoppedOut(actions::kActionCut, true);
  CheckIsPinned(actions::kActionCut, false);
  // Verify setting as not ephemerally visible remove the popped out button.
  container()->ShowActionEphemerallyInToolbar(actions::kActionCut, false);
  CheckIsPoppedOut(actions::kActionCut, false);
  CheckIsPinned(actions::kActionCut, false);
}