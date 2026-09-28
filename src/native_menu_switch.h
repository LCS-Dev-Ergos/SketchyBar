#pragma once

#include <stdbool.h>
#include <CoreGraphics/CoreGraphics.h>

// The built-in display's MenuBarAgent covers the entire menu-bar strip. Keep
// both bar layers strictly above its status window until the cursor reaches
// the screen edge; then put both below it without moving either bar.
#define NATIVE_MENU_RAISED_ITEM_LEVEL (kCGStatusWindowLevel + 2)
#define NATIVE_MENU_ENTER_DEPTH 8.0
#define NATIVE_MENU_EXIT_DEPTH 48.0
#define NATIVE_MENU_EXIT_DELAY 0.20

struct native_menu_switch {
  bool revealed;
  double exit_after;
};

static inline int native_menu_switch_level(bool enabled,
                                           bool builtin,
                                           bool revealed,
                                           int configured_level) {
  if (!enabled || !builtin) return configured_level;
  return revealed ? kCGFloatingWindowLevel : NATIVE_MENU_RAISED_ITEM_LEVEL;
}

// Returns true only when a WindowServer level change is needed. A cursor in
// the native menu strip keeps it exposed; moving below the strip must persist
// briefly before SketchyBar retakes clicks.
static inline bool native_menu_switch_step(struct native_menu_switch* state,
                                           bool on_display,
                                           double depth,
                                           double now,
                                           bool popup_open) {
  if (!on_display) {
    state->exit_after = 0;
    if (!state->revealed) return false;
    state->revealed = false;
    return true;
  }

  if (depth <= NATIVE_MENU_ENTER_DEPTH) {
    state->exit_after = 0;
    if (state->revealed) return false;
    state->revealed = true;
    return true;
  }

  if (!state->revealed) return false;
  if (depth < NATIVE_MENU_EXIT_DEPTH) {
    state->exit_after = 0;
    return false;
  }

  if (state->exit_after == 0) {
    state->exit_after = now + NATIVE_MENU_EXIT_DELAY;
    return false;
  }
  if (now < state->exit_after) return false;
  if (popup_open) {
    state->exit_after = now + NATIVE_MENU_EXIT_DELAY;
    return false;
  }

  state->exit_after = 0;
  state->revealed = false;
  return true;
}
