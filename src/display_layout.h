#pragma once
#include <CoreGraphics/CoreGraphics.h>
#include <stdbool.h>
#include <string.h>

// The display topology the bar windows were built for. Wake and hot-plug
// notifications arrive in bursts, mostly for a topology that ends up
// unchanged; comparing snapshots lets SketchyBar keep its windows instead of
// recreating every bar and item window for each notification.
#define DISPLAY_LAYOUT_MAX 16

struct display_layout {
  uint32_t count;
  uint32_t did[DISPLAY_LAYOUT_MAX];
  CGRect bounds[DISPLAY_LAYOUT_MAX];
};

enum display_reconcile {
  DISPLAY_RECONCILE_WAIT,     // Layout in transition; a later callback follows.
  DISPLAY_RECONCILE_KEEP,     // Unchanged behind the lock screen; unlock refreshes.
  DISPLAY_RECONCILE_REFRESH,  // Unchanged; keep the windows and redraw them.
  DISPLAY_RECONCILE_REBUILD,  // Changed; recreate the bars once.
};

// Reads the active displays; an empty layout when they cannot be listed.
void display_layout_read(struct display_layout* layout);

// The session dictionary only carries its lock key while the screen is locked.
bool display_session_locked(void);

// A display that is being reprobed reports no bounds; its final state is
// announced by a later reconfiguration callback.
static inline bool display_layout_usable(const struct display_layout* layout) {
  if (layout->count == 0) return false;
  for (uint32_t i = 0; i < layout->count; i++) {
    if (!layout->did[i] || CGRectIsEmpty(layout->bounds[i])) return false;
  }
  return true;
}

static inline bool display_layout_equal(const struct display_layout* a,
                                        const struct display_layout* b) {
  if (a->count != b->count) return false;
  for (uint32_t i = 0; i < a->count; i++) {
    if (a->did[i] != b->did[i]
        || !CGRectEqualToRect(a->bounds[i], b->bounds[i])) {
      return false;
    }
  }
  return true;
}

// While settling after a wake, macOS may briefly replace the real displays
// with a placeholder before reprobing them. A different topology is only
// trusted once the settle period has passed. A wake usually lands on the lock
// screen, and the unlock that follows refreshes the windows anyway.
static inline enum display_reconcile display_reconcile_decide(
                                        const struct display_layout* built,
                                        const struct display_layout* current,
                                        bool has_bars,
                                        bool settling,
                                        bool locked) {
  if (!display_layout_usable(current)) return DISPLAY_RECONCILE_WAIT;
  if (!has_bars) return DISPLAY_RECONCILE_REBUILD;
  if (display_layout_equal(built, current)) {
    return locked ? DISPLAY_RECONCILE_KEEP : DISPLAY_RECONCILE_REFRESH;
  }
  return settling ? DISPLAY_RECONCILE_WAIT : DISPLAY_RECONCILE_REBUILD;
}
