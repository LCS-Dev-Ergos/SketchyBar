#pragma once

#include <CoreGraphics/CoreGraphics.h>

// A click on the bar background brings it to the front of the windows at its
// level, above the item windows, which WindowServer does since macOS 27. One
// level below the items keeps the background behind them whatever the order
// within a level becomes.
static inline int bar_background_level(int item_level) {
  return item_level - 1;
}

// topmost=on draws the bar over the menu bar. Its items sit one level above
// the status level, so that the background stays at the status level, above
// the menu bar, as the whole bar did before it was split.
#define BAR_TOPMOST_ITEM_LEVEL (kCGStatusWindowLevel + 1)
