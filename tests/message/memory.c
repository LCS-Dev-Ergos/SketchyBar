// Commands that corrupted memory before: --move of an item relative to itself
// left an unset slot in the item list, an image path starting with ~ was read
// after it was freed, and a clone shared the graph samples and alias names of
// its parent, which removing both freed twice. ASan reports the last two.
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "harness.h"

#define SEND(...) free(harness_send(__VA_ARGS__, NULL))

static struct bar_item* item(const char* name) {
  int index = bar_manager_get_item_index_for_name(&g_bar_manager, (char*)name);
  assert(index >= 0);
  return g_bar_manager.bar_items[index];
}

int main(void) {
  harness_begin();
  SEND("--add", "item", "a", "left");
  SEND("--add", "item", "b", "left");
  SEND("--add", "item", "c", "left");

  // Moving an item relative to itself keeps the list as it is.
  struct bar_item* a = item("a");
  struct bar_item* b = item("b");
  struct bar_item* c = item("c");
  SEND("--move", "b", "before", "b");
  SEND("--move", "b", "after", "b");
  assert(g_bar_manager.bar_item_count == 3);
  assert(g_bar_manager.bar_items[0] == a);
  assert(g_bar_manager.bar_items[1] == b);
  assert(g_bar_manager.bar_items[2] == c);

  // Moving relative to another item still works.
  SEND("--move", "a", "after", "c");
  assert(g_bar_manager.bar_items[0] == b);
  assert(g_bar_manager.bar_items[1] == c);
  assert(g_bar_manager.bar_items[2] == a);

  // A missing image in the home directory is reported with its full path.
  char* response = harness_send("--set", "a", "background.image=~/sketchybar-tests-missing.png", NULL);
  char expected[1024];
  snprintf(expected, sizeof(expected), "'%s/sketchybar-tests-missing.png' not found", getenv("HOME"));
  assert(strstr(response, expected));
  free(response);

  // A clone of a graph gets its own samples, with the values of its parent.
  SEND("--add", "graph", "load", "left", "4");
  SEND("--push", "load", "0.5", "0.25");
  SEND("--clone", "load.copy", "load");
  struct bar_item* load = item("load");
  struct bar_item* copy = item("load.copy");
  assert(copy->graph.y && copy->graph.y != load->graph.y);
  assert(copy->graph.width == 4);
  assert(memcmp(copy->graph.y, load->graph.y, 4 * sizeof(float)) == 0);
  SEND("--remove", "load");
  SEND("--remove", "load.copy");

  // A clone of an alias gets its own names.
  SEND("--add", "alias", "SketchyBarTests,Missing", "right");
  SEND("--clone", "alias.copy", "SketchyBarTests,Missing");
  struct bar_item* alias = item("SketchyBarTests,Missing");
  struct bar_item* alias_copy = item("alias.copy");
  assert(alias_copy->alias.owner && alias_copy->alias.owner != alias->alias.owner);
  assert(strcmp(alias_copy->alias.owner, alias->alias.owner) == 0);
  SEND("--remove", "SketchyBarTests,Missing");
  SEND("--remove", "alias.copy");

  assert(g_bar_manager.bar_item_count == 3);
  printf("message memory: passed\n");
  return 0;
}
