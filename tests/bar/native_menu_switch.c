#include <assert.h>
#include <stdio.h>

#include "native_menu_switch.h"
#include "bar_level.h"

int main(void) {
  struct native_menu_switch state = { 0 };
  const double start = 100.0;

  // Ordinary clicks in the menu-bar strip belong to SketchyBar until the
  // cursor reaches the screen edge.
  assert(!native_menu_switch_step(&state, true, 20, start, false));
  assert(!state.revealed);
  int raised = native_menu_switch_level(true, true, state.revealed,
                                        kCGFloatingWindowLevel);
  assert(bar_background_level(raised) > kCGStatusWindowLevel);
  assert(raised < kCGPopUpMenuWindowLevel);

  assert(native_menu_switch_step(&state, true, 2, start + 0.04, false));
  assert(state.revealed);
  int lowered = native_menu_switch_level(true, true, state.revealed,
                                         kCGFloatingWindowLevel);
  assert(lowered < kCGStatusWindowLevel);
  assert(!native_menu_switch_step(&state, true, 20, start + 0.08, false));

  // Brief movement into a menu does not immediately steal its heading back.
  assert(!native_menu_switch_step(&state, true, 65, start + 0.12, false));
  assert(!native_menu_switch_step(&state, true, 65, start + 0.25, false));
  assert(state.revealed);
  assert(!native_menu_switch_step(&state, true, 20, start + 0.26, false));
  assert(!native_menu_switch_step(&state, true, 65, start + 0.30, false));
  assert(!native_menu_switch_step(&state, true, 65, start + 0.51, true));
  assert(state.revealed);
  assert(native_menu_switch_step(&state, true, 65, start + 0.72, false));
  assert(!state.revealed);

  // Leaving the display restores the bar; external bars retain their level.
  assert(native_menu_switch_step(&state, true, 0, start + 1.0, false));
  assert(native_menu_switch_step(&state, false, -1, start + 1.1, false));
  assert(native_menu_switch_level(true, false, false,
                                  kCGFloatingWindowLevel)
         == kCGFloatingWindowLevel);
  assert(native_menu_switch_level(false, true, false,
                                  kCGFloatingWindowLevel)
         == kCGFloatingWindowLevel);

  puts("native menu switch: passed");
  return 0;
}
