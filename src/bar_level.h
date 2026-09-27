#pragma once

// A click on the bar background brings it to the front of the windows at its
// level, above the item windows, which WindowServer does since macOS 27. One
// level below the items keeps the background behind them whatever the order
// within a level becomes.
static inline int bar_background_level(int item_level) {
  return item_level - 1;
}
