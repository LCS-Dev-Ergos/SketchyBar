// Runs the display reconciliation of src/display_reconcile.c on the main
// dispatch queue against counting stubs of the bar manager and simulated
// display layouts. The build shortens the quiet and settle periods.
//
// The checks wait for events instead of fixed times, so that a slow or
// sanitized runner, whose timers macOS may delay, does not fail them.
#include <assert.h>
#include <stdio.h>
#include <unistd.h>

#include "bar_manager.h"
#include "event.h"
#include "display_layout.h"
#include "display_reconcile.h"

struct bar_manager g_bar_manager;
static struct bar g_bar;
static struct display_layout g_layout;
static bool g_locked;
static int posts, refreshes, rebuilds, wakes, woke_events;
static uint64_t last_post;

static uint64_t now(void) {
  return clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
}

void display_layout_read(struct display_layout* layout) {
  *layout = g_layout;
}

bool display_session_locked(void) {
  return g_locked;
}

void event_post(struct event* event) {
  assert(event->type == DISPLAY_RECONCILE);
  posts++;
  last_post = now();
  display_reconcile_run(&g_bar_manager, false);
}

void bar_manager_display_changed(struct bar_manager* bar_manager) {
  rebuilds++;
  // Rebuilding creates the bars again through bar_manager_begin.
  display_reconcile_remember();
}

void bar_manager_refresh(struct bar_manager* bar_manager, bool forced) {
  refreshes++;
}

void bar_manager_handle_system_woke(struct bar_manager* bar_manager) {
  wakes++;
  display_reconcile_wake(bar_manager);
  woke_events++;
}

void bar_manager_custom_events_trigger(struct bar_manager* bar_manager, char* name, struct env_vars* env_vars) {
  assert(string_equals(name, COMMAND_SUBSCRIBE_SYSTEM_WOKE));
  woke_events++;
}

void bar_manager_handle_display_change(struct bar_manager* bar_manager) { }
void bar_manager_handle_space_change(struct bar_manager* bar_manager, bool forced) { }
void animator_renew_display_link(struct animator* animator) { }

static void run(double seconds) {
  CFRunLoopRunInMode(kCFRunLoopDefaultMode, seconds, false);
}

// Runs the main queue until the condition holds, for at most five seconds,
// then a little longer so that a reconciliation that should not follow has
// the time to show up.
static void run_until(int (^condition)(void)) {
  uint64_t deadline = now() + 5 * NSEC_PER_SEC;
  while (!condition() && now() < deadline) run(0.01);
  assert(condition());
  run(0.1);
}

static void reset(void) {
  posts = refreshes = rebuilds = wakes = woke_events = 0;
  g_bar.window.needs_move = false;
}

int main(void) {
  static struct bar* bars[] = { &g_bar };
  g_bar.adid = 1;
  g_bar_manager.bars = bars;
  g_bar_manager.bar_count = 1;

  const struct display_layout desk = { 2, { 3, 2 },
                                       { { { 0, 0 }, { 3008, 1692 } },
                                         { { 3008, -785 }, { 1692, 3008 } } } };
  const struct display_layout placeholder = { 1, { 5 },
                                              { { { 0, 0 }, { 1920, 1080 } } } };

  g_layout = desk;
  display_reconcile_remember();

  // A burst of display callbacks reconciles once and keeps the windows.
  reset();
  for (int i = 0; i < 4; i++) display_reconcile_request();
  run_until(^{ return posts > 0; });
  assert(posts == 1 && refreshes == 1 && rebuilds == 0);
  assert(g_bar.window.needs_move);

  // Every request postpones the reconciliation by the whole quiet period. The
  // main queue does not run between the requests, so the first one cannot
  // reconcile before the second arrives, however long the runner sleeps.
  reset();
  display_reconcile_request();
  usleep(20000);
  uint64_t second = now();
  display_reconcile_request();
  run_until(^{ return posts > 0; });
  assert(posts == 1 && refreshes == 1);
  assert(last_post - second >= DISPLAY_RECONCILE_QUIET_NS - NSEC_PER_MSEC);

  // A changed layout outside the settle period is rebuilt once.
  reset();
  g_layout = placeholder;
  display_reconcile_request();
  run_until(^{ return posts > 0; });
  assert(posts == 1 && rebuilds == 1 && refreshes == 0);
  g_layout = desk;
  display_reconcile_remember();

  // After a wake a placeholder is not trusted; the desk returns within the
  // settle period and is refreshed at its end.
  reset();
  g_bar_manager.sleeps = true;
  g_layout = placeholder;
  uint64_t woke = now();
  display_reconcile_wake(&g_bar_manager);
  assert(!g_bar_manager.sleeps);
  run_until(^{ return posts > 0; });
  assert(last_post - woke < DISPLAY_WAKE_SETTLE_NS);
  assert(posts == 1 && refreshes == 0 && rebuilds == 0);
  g_layout = desk;
  run_until(^{ return posts > 1; });
  assert(posts == 2 && refreshes == 1 && rebuilds == 0);
  assert(last_post - woke >= DISPLAY_WAKE_SETTLE_NS - NSEC_PER_MSEC);

  // A layout that is still different when the settle period ends is rebuilt.
  reset();
  g_bar_manager.sleeps = true;
  g_layout = placeholder;
  display_reconcile_wake(&g_bar_manager);
  run_until(^{ return rebuilds > 0; });
  assert(posts == 2 && rebuilds == 1 && refreshes == 0);
  g_layout = desk;
  display_reconcile_remember();

  // Behind the lock screen the unchanged desk waits for the unlock.
  reset();
  g_locked = true;
  display_reconcile_request();
  run_until(^{ return posts > 0; });
  assert(posts == 1 && refreshes == 0 && rebuilds == 0);
  g_locked = false;
  display_reconcile_unlock(&g_bar_manager);
  assert(refreshes == 1 && rebuilds == 0 && woke_events == 1);

  // An unlock reconciles at once and drops the reconciliation still pending.
  reset();
  display_reconcile_request();
  display_reconcile_unlock(&g_bar_manager);
  assert(refreshes == 1 && woke_events == 1);
  run(0.3);
  assert(posts == 0 && refreshes == 1);

  // An unlock that follows a sleep takes the wake path.
  reset();
  g_bar_manager.sleeps = true;
  display_reconcile_unlock(&g_bar_manager);
  assert(wakes == 1 && woke_events == 1 && refreshes == 0);
  run_until(^{ return posts > 0; });
  assert(posts == 1 && refreshes == 1 && rebuilds == 0);

  // Nothing is reconciled while the system sleeps.
  reset();
  g_bar_manager.sleeps = true;
  display_reconcile_request();
  run_until(^{ return posts > 0; });
  assert(posts == 1 && refreshes == 0 && rebuilds == 0);

  printf("display reconciliation: passed\n");
  return 0;
}
