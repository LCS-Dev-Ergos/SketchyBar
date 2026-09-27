// Window levels of the bar background (src/bar_level.h) for the item levels
// bar_manager_set_topmost selects: off, window and on.
#include <assert.h>
#include <stdio.h>
#include <CoreGraphics/CoreGraphics.h>

#include "bar_level.h"

int main(void) {
  const int item_levels[] = { kCGBackstopMenuLevel,
                              kCGFloatingWindowLevel,
                              kCGStatusWindowLevel    };

  // The background sits below the items at every topmost setting, so a click
  // that raises it within its level cannot cover them.
  for (int i = 0; i < 3; i++) {
    assert(bar_background_level(item_levels[i]) < item_levels[i]);
  }

  // topmost=off keeps the background above the desktop and its icons.
  assert(bar_background_level(kCGBackstopMenuLevel) > kCGDesktopIconWindowLevel);

  // topmost=window keeps it above ordinary application windows.
  assert(bar_background_level(kCGFloatingWindowLevel) > kCGNormalWindowLevel);

  // topmost=on leaves it at the menu bar's own level, not below it; only the
  // items are above the menu bar there.
  assert(bar_background_level(kCGStatusWindowLevel) == kCGMainMenuWindowLevel);

  printf("bar background levels: passed\n");
  return 0;
}
