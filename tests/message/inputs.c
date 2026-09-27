// Sends commands with out-of-range values to the daemon's message handlers:
// numbers as long as the message, indices beyond the 32-bit masks, repeated
// --reorder names, a graph of width 0 and more than 64 events. Each check
// failed before its fix: the long number in every build, the others under
// ASan and UBSan.
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "harness.h"

static struct bar_item* item(const char* name) {
  int index = bar_manager_get_item_index_for_name(&g_bar_manager, (char*)name);
  assert(index >= 0);
  return g_bar_manager.bar_items[index];
}

#define SEND(...) free(harness_send(__VA_ARGS__, NULL))

int main(void) {
  harness_begin();
  SEND("--add", "item", "clock", "left");
  SEND("--add", "item", "date", "left");
  SEND("--add", "item", "battery", "right");

  // A number as long as the message is parsed from a bounded copy instead of
  // a stack array of its length, which overflowed the main thread's stack.
  size_t length = 16 << 20;
  char* width = malloc(length + 7);
  memcpy(width, "width=", 6);
  memset(width + 6, '9', length);
  width[length + 6] = '\0';
  SEND("--set", "clock", width);
  free(width);

  // Space and display indices the masks cannot hold select nothing.
  SEND("--set", "clock", "space=40", "display=33");
  assert(item("clock")->associated_space == 0);
  assert(item("clock")->associated_display == 0);
  SEND("--set", "clock", "space=1,2", "display=2");
  assert(item("clock")->associated_space == 0x6);
  assert(item("clock")->associated_display == 0x4);

  // Repeated names keep their first position and cannot overflow the list.
  SEND("--reorder", "battery", "battery", "clock", "battery", "clock", "date", "date");
  assert(g_bar_manager.bar_item_count == 3);
  assert(g_bar_manager.bar_items[0] == item("battery"));
  assert(g_bar_manager.bar_items[1] == item("clock"));
  assert(g_bar_manager.bar_items[2] == item("date"));

  // A graph of width 0 ignores its samples.
  SEND("--add", "graph", "load", "left", "0");
  SEND("--push", "load", "0.5", "0.25");
  assert(item("load")->graph.width == 0);

  // Events beyond the 64 bits of a subscription mask are refused.
  char name[16];
  for (int i = 0; i < 70; i++) {
    snprintf(name, sizeof(name), "event_%d", i);
    SEND("--add", "event", name);
  }
  assert(g_bar_manager.custom_events.count == 64);
  assert(custom_events_get_flag_for_name(&g_bar_manager.custom_events, "event_45") == 1ULL << 63);
  assert(custom_events_get_flag_for_name(&g_bar_manager.custom_events, "event_46") == 0);

  printf("message inputs: passed\n");
  return 0;
}
