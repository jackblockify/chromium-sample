// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_MULTI_WORD_SUGGESTER_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_MULTI_WORD_SUGGESTER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/input_method/suggester.h"
#include "chrome/browser/ash/input_method/suggestion_enums.h"
#include "chrome/browser/ash/input_method/suggestion_handler_interface.h"
#include "chromeos/ash/services/ime/public/cpp/assistive_suggestions.h"

namespace ash {
namespace input_method {

// Integrates multi word suggestions produced by the system with the assistive
// framework. Handles showing / accepting / dismissing any multi word
// suggestions generated by the system.
class MultiWordSuggester : public Suggester {
 public:
  // `suggestion_handler` and `profile` need to exist longer than the lifetime
  // of this object.
  MultiWordSuggester(SuggestionHandlerInterface* suggestion_handler,
                     Profile* profile);
  ~MultiWordSuggester() override;

  // Suggester overrides:
  void OnFocus(int context_id) override;
  void OnBlur() override;
  void OnExternalSuggestionsUpdated(
      const std::vector<ime::AssistiveSuggestion>& suggestions,
      const std::optional<ime::SuggestionsTextContext>& context) override;
  SuggestionStatus HandleKeyEvent(const ui::KeyEvent& event) override;
  bool TrySuggestWithSurroundingText(const std::u16string& text,
                                     gfx::Range selection_range) override;
  bool AcceptSuggestion(size_t index = 0) override;
  void DismissSuggestion() override;
  AssistiveType GetProposeActionType() override;
  bool HasSuggestions() override;
  std::vector<ime::AssistiveSuggestion> GetSuggestions() override;

  // Used to capture any changes to the current input text.
  void OnSurroundingTextChanged(const std::u16string& text,
                                gfx::Range selection_range);

 private:
  // Used to capture any internal state around the previously or currently
  // shown suggestions.
  class SuggestionState {
   public:
    enum State {
      kNoSuggestionShown,
      kPredictionSuggestionShown,
      kCompletionSuggestionShown,
      kTrackingLastSuggestionShown,
      kSuggestionDismissed,
      kSuggestionAccepted,
    };

    struct Suggestion {
      ime::AssistiveSuggestionMode mode;
      std::u16string text;
      size_t confirmed_length;
      size_t initial_confirmed_length;
      base::TimeTicks time_first_shown;
      bool highlighted = false;
      size_t original_surrounding_text_length;
    };

    struct SurroundingText {
      std::u16string text;
      size_t cursor_pos;
      size_t prev_cursor_pos;
      bool cursor_at_end_of_text;
    };

    explicit SuggestionState(MultiWordSuggester* suggester);
    ~SuggestionState();

    // As the name suggests, used to update the current state and perform
    // any actions required during a transition.
    void UpdateState(const State& state);

    // Captures surrounding text context.
    void UpdateSurroundingText(const SurroundingText& surrounding_text);

    // Captures new suggestion context.
    void UpdateSuggestion(const Suggestion& suggestion,
                          bool new_tracking_behavior);

    // Validates the given suggestion text context with the current surrounding
    // text, and returns the state of the given suggestion context.
    MultiWordSuggestionState ValidateSuggestion(
        const Suggestion& suggestion,
        const ime::SuggestionsTextContext& context);

    // Takes the current suggestion and surrounding text state, and ensures the
    // confirmed length or any other suggestion details are correct.
    void ReconcileSuggestionWithText();

    // Toggles the highlight state of the current suggeston.
    void ToggleSuggestionHighlight();

    // As the name suggests, is there a suggestion currently shown to the user?
    bool IsSuggestionShowing();

    // Is the user's cursor located at the end of the text they are currently
    // editing?
    bool IsCursorAtEndOfText();

    // Has the user highlighted the current suggestion showing?
    bool IsSuggestionHighlighted();

    // Returns the current suggestion state if there is any available.
    std::optional<Suggestion> GetSuggestion();

    // Returns the last suggestion type shown to the user. This suggestion may,
    // or may not, be currently showing to the user.
    AssistiveType GetLastSuggestionType();

    // Resets the current suggestion context, and any other related state.
    void ResetSuggestion();

   private:
    // Not owned by this class
    raw_ptr<MultiWordSuggester> suggester_;

    // The current state of the suggester (eg is a suggestion shown or not).
    State state_ = State::kNoSuggestionShown;

    // Last known surrounding text context captured by the suggester.
    std::optional<SurroundingText> surrounding_text_;

    // The current suggestion shown to the user by the suggester.
    std::optional<Suggestion> suggestion_;

    // The last suggestion type shown to the user.
    AssistiveType last_suggestion_type_ = AssistiveType::kGenericAction;
  };

  void DisplaySuggestionIfAvailable();
  void DisplaySuggestion(const SuggestionState::Suggestion& suggestion);
  void ResetSuggestionState();
  void ResetTextState();

  // Visibly highlight the suggestion in the ui shown to the user.
  void SetSuggestionHighlight(bool highlighted);

  // Announce the given message to the user.
  void Announce(const std::u16string& message);

  // The currently focused input (nullopt if none are focused)
  std::optional<int> focused_context_id_;

  // Not owned by this class
  raw_ptr<SuggestionHandlerInterface, DanglingUntriaged> suggestion_handler_;

  // Current suggestion state
  SuggestionState state_;

  ui::ime::AssistiveWindowButton suggestion_button_;

  // The current user's Chrome user profile.
  const raw_ptr<Profile> profile_;
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_MULTI_WORD_SUGGESTER_H_
