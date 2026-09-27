// Decision table of the display reconciliation (src/display_layout.h),
// including the display sequence recorded while two external displays woke
// from sleep: no displays, unsized displays, a placeholder, then the desk.
#include <assert.h>
#include <stdio.h>

#include "display_layout.h"

// Builds a layout from parallel arrays of display ids and bounds.
static struct display_layout layout(uint32_t count,
                                    const uint32_t* did,
                                    const CGRect* bounds) {
  struct display_layout result = { .count = count };
  for (uint32_t i = 0; i < count; i++) {
    result.did[i] = did[i];
    result.bounds[i] = bounds[i];
  }
  return result;
}

int main(void) {
  const uint32_t desk_did[] = { 3, 2 };
  const CGRect desk_bounds[] = { { { 0, 0 }, { 3008, 1692 } },
                                 { { 3008, -785 }, { 1692, 3008 } } };
  const uint32_t placeholder_did[] = { 5 };
  const CGRect placeholder_bounds[] = { { { 0, 0 }, { 1920, 1080 } } };
  const CGRect reprobing_bounds[] = { CGRectZero, CGRectZero };

  struct display_layout built = layout(2, desk_did, desk_bounds);
  struct display_layout same = layout(2, desk_did, desk_bounds);
  struct display_layout empty = { 0 };
  struct display_layout reprobing = layout(2, desk_did, reprobing_bounds);
  struct display_layout placeholder = layout(1, placeholder_did, placeholder_bounds);

  // The observed wake: nothing is rebuilt and the desk is refreshed once.
  assert(display_reconcile_decide(&built, &empty, true, true, false)
         == DISPLAY_RECONCILE_WAIT);
  assert(display_reconcile_decide(&built, &reprobing, true, true, false)
         == DISPLAY_RECONCILE_WAIT);
  assert(display_reconcile_decide(&built, &placeholder, true, true, false)
         == DISPLAY_RECONCILE_WAIT);
  assert(display_reconcile_decide(&built, &same, true, true, false)
         == DISPLAY_RECONCILE_REFRESH);

  // Behind the lock screen the unchanged desk waits for the unlock refresh,
  // while a changed layout outside the settle period is rebuilt.
  assert(display_reconcile_decide(&built, &same, true, true, true)
         == DISPLAY_RECONCILE_KEEP);
  assert(display_reconcile_decide(&built, &placeholder, true, false, true)
         == DISPLAY_RECONCILE_REBUILD);

  // Outside the settle period a different layout is rebuilt at once.
  assert(display_reconcile_decide(&built, &same, true, false, false)
         == DISPLAY_RECONCILE_REFRESH);
  assert(display_reconcile_decide(&built, &placeholder, true, false, false)
         == DISPLAY_RECONCILE_REBUILD);

  // Without bars any usable layout is built, and an unusable one waits.
  assert(display_reconcile_decide(&built, &same, false, true, false)
         == DISPLAY_RECONCILE_REBUILD);
  assert(display_reconcile_decide(&built, &empty, false, false, false)
         == DISPLAY_RECONCILE_WAIT);

  // A moved display is a changed layout.
  struct display_layout moved = same;
  moved.bounds[1].origin.y = 0;
  assert(!display_layout_equal(&built, &moved));
  assert(display_reconcile_decide(&built, &moved, true, false, false)
         == DISPLAY_RECONCILE_REBUILD);

  // So is a display id reported for the same position.
  struct display_layout renumbered = same;
  renumbered.did[0] = 7;
  assert(display_reconcile_decide(&built, &renumbered, true, false, false)
         == DISPLAY_RECONCILE_REBUILD);

  // An id of zero never counts as usable.
  struct display_layout unnamed = same;
  unnamed.did[1] = 0;
  assert(!display_layout_usable(&unnamed));

  printf("display layout decisions: passed\n");
  return 0;
}
