// Sets properties while no animation runs, which cancels the running
// animations of each property. The animator's lists of animations to remove
// had length 0 then, which UBSan reports.
#include <assert.h>
#include <stdio.h>

#include "harness.h"

#define SEND(...) free(harness_send(__VA_ARGS__, NULL))

int main(void) {
  harness_begin();
  SEND("--add", "item", "clock", "left");
  SEND("--set", "clock", "y_offset=5", "width=40", "label=12:00");

  int index = bar_manager_get_item_index_for_name(&g_bar_manager, "clock");
  assert(index >= 0);
  assert(g_bar_manager.bar_items[index]->y_offset == 5);
  assert(g_bar_manager.bar_items[index]->custom_width == 40);
  assert(g_bar_manager.animator.animation_count == 0);

  printf("animator without animations: passed\n");
  return 0;
}
