// Removes an item while its properties animate. Its animations kept running
// and wrote into the freed item on the next frame, which ASan reports.
#include <assert.h>
#include <stdio.h>
#include <mach/mach_time.h>

#include "harness.h"

#define SEND(...) free(harness_send(__VA_ARGS__, NULL))

int main(void) {
  harness_begin();
  SEND("--add", "item", "clock", "left");
  SEND("--add", "item", "date", "left");
  SEND("--animate", "linear", "30",
       "--set", "clock", "width=100", "icon.y_offset=10",
       "--set", "date", "width=50");
  uint32_t running = g_bar_manager.animator.animation_count;
  assert(running >= 3);

  // Only the animations of the other item remain, and the next frame runs
  // them alone.
  SEND("--remove", "clock");
  assert(g_bar_manager.animator.animation_count > 0);
  assert(g_bar_manager.animator.animation_count < running);
  animator_update(&g_bar_manager.animator, mach_absolute_time());

  SEND("--remove", "date");
  assert(g_bar_manager.animator.animation_count == 0);

  printf("removal during animations: passed\n");
  return 0;
}
