// Replays a post-decode frame callback after its embedded window was cleared
// and its owner freed. No real window or WindowServer connection is used.
#include <assert.h>
#include <Block.h>
#include <stdio.h>

#include "../../src/window.c"
#include "../../src/window_reuse.c"

struct bar_manager g_bar_manager;
int g_connection;
int64_t g_disable_capture;
CFTypeRef g_transaction;
CGError (* SBSLSTransactionAddPostDecodeAction)(CFTypeRef, void (^)());

struct surface* surface_create(struct window* window) { return NULL; }
void surface_destroy(struct surface* surface) { }
void surface_resize(struct surface* surface, struct window* window) { }
void surface_flush(struct surface* surface) { }
void layer_set_bounds(struct layer* layer, CGRect bounds) { }

static void (^post_decode_actions[8])(void);
static int action_count;

__attribute__((no_sanitize("function")))
static CGError record_post_decode(CFTypeRef transaction, void (^action)(void)) {
  assert(action_count < 8);
  post_decode_actions[action_count++] = Block_copy(action);
  return kCGErrorSuccess;
}

static void deliver_action(int index) {
  assert(index < action_count && post_decode_actions[index]);
  post_decode_actions[index]();
  Block_release(post_decode_actions[index]);
  post_decode_actions[index] = NULL;
}

static void run_main_queue(void) {
  __block bool finished = false;
  dispatch_async(dispatch_get_main_queue(), ^{ finished = true; });
  for (int i = 0; i < 500 && !finished; i++) {
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, false);
  }
  assert(finished);
}

int main(void) {
  SBSLSTransactionAddPostDecodeAction = record_post_decode;
  struct window* embedded = window_create();
  window_schedule_update(embedded, true);
  assert(action_count == 1);

  window_close(embedded);
  free(embedded);
  deliver_action(0);
  run_main_queue();

  // Reopening the same embedded storage must not let an old callback update
  // the new window or change its reference count.
  embedded = window_create();
  window_schedule_update(embedded, true);
  window_clear(embedded);
  window_schedule_update(embedded, true);
  deliver_action(1);
  run_main_queue();
  assert(embedded->refc == 2);
  deliver_action(2);
  run_main_queue();
  assert(embedded->refc == 1);
  window_destroy(embedded);

  // A heap window removed while an update is pending is released by the
  // final callback, including when it has no surface.
  struct window* heap_window = window_create();
  window_schedule_update(heap_window, true);
  window_destroy(heap_window);
  deliver_action(3);
  run_main_queue();

  // Reload transfers resources immediately and cancels a pending callback.
  heap_window = window_create();
  window_schedule_update(heap_window, true);
  struct window_deferred_update* update = heap_window->deferred_update;
  windows_reuse_begin();
  window_destroy(heap_window);
  assert(update->window == NULL);
  windows_reuse_end();
  deliver_action(4);
  run_main_queue();

  puts("window deferred update: passed");
  return 0;
}
