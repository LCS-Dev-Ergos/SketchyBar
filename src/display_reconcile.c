#include "display_reconcile.h"
#include "display_layout.h"
#include "bar_manager.h"
#include "event.h"

// Coalesce display notifications: a wake produces several CoreGraphics
// callbacks, and each rebuild recreates every bar and item window.
#ifndef DISPLAY_RECONCILE_QUIET_NS
#define DISPLAY_RECONCILE_QUIET_NS (150 * NSEC_PER_MSEC)
#endif

#ifndef DISPLAY_WAKE_SETTLE_NS
#define DISPLAY_WAKE_SETTLE_NS (6 * NSEC_PER_SEC)
#endif

static struct display_layout g_display_layout;
static uint64_t g_display_generation = 0;
static uint64_t g_display_settle_until = 0;

void display_reconcile_remember(void) {
  display_layout_read(&g_display_layout);
}

// Runs after the notification burst instead of inside the CoreGraphics
// callback, which WindowServer waits on while it reconfigures the displays.
static void display_reconcile_schedule(uint64_t delay) {
  uint64_t generation = ++g_display_generation;
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, delay),
                 dispatch_get_main_queue(), ^{
    if (generation != g_display_generation) return;
    struct event event = { NULL, DISPLAY_RECONCILE };
    event_post(&event);
  });
}

void display_reconcile_request(void) {
  display_reconcile_schedule(DISPLAY_RECONCILE_QUIET_NS);
}

static void display_reconcile_move_windows_to_origin(struct bar_manager* bar_manager) {
  for (uint32_t i = 0; i < bar_manager->bar_count; i++) {
    struct bar* bar = bar_manager->bars[i];
    bar->window.needs_move = true;
    for (uint32_t j = 0; j < bar_manager->bar_item_count; j++) {
      struct bar_item* bar_item = bar_manager->bar_items[j];
      if (bar->adid < 1 || bar->adid > bar_item->num_windows) continue;
      struct window* window = bar_item->windows[bar->adid - 1];
      if (window) window->needs_move = true;
    }
  }
}

void display_reconcile_run(struct bar_manager* bar_manager, bool unlocked) {
  if (bar_manager->sleeps) return;
  g_display_generation++;

  uint64_t start = clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
  bool settling = start < g_display_settle_until;
  struct display_layout layout;
  display_layout_read(&layout);

  enum display_reconcile action = display_reconcile_decide(&g_display_layout,
                                                           &layout,
                                                           bar_manager->bar_count > 0,
                                                           settling,
                                                           !unlocked
                                                           && display_session_locked());
  const char* outcome = "waiting";
  if (action == DISPLAY_RECONCILE_WAIT) {
    // A transient layout gets one more look when the settle period ends.
    if (settling) {
      display_reconcile_schedule(g_display_settle_until - start);
    }
  } else if (action == DISPLAY_RECONCILE_KEEP) {
    g_display_settle_until = 0;
    outcome = "unchanged, refresh left to unlock";
  } else if (action == DISPLAY_RECONCILE_REFRESH) {
    g_display_settle_until = 0;
    outcome = "unchanged, refreshed";
    // WindowServer may relocate windows while a display is reprobed.
    display_reconcile_move_windows_to_origin(bar_manager);
    bar_manager_handle_display_change(bar_manager);
    bar_manager_handle_space_change(bar_manager, false);
    bar_manager->needs_ordering = true;
    bar_manager_refresh(bar_manager, true);
    animator_renew_display_link(&bar_manager->animator);
  } else {
    g_display_settle_until = 0;
    outcome = "changed, rebuilt";
    bar_manager_display_changed(bar_manager);
  }

  uint64_t end = clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
  time_t now = time(NULL);
  char stamp[32];
  strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
  fprintf(stderr, "%s sketchybar: displays %s (%u active%s) in %llu ms\n",
                  stamp,
                  outcome,
                  layout.count,
                  settling ? ", settling" : "",
                  (end - start) / NSEC_PER_MSEC                         );
}

void display_reconcile_wake(struct bar_manager* bar_manager) {
  if (bar_manager->sleeps) {
    bar_manager->sleeps = false;
    g_display_settle_until = clock_gettime_nsec_np(CLOCK_UPTIME_RAW)
                             + DISPLAY_WAKE_SETTLE_NS;
  }

  // Display reconfiguration callbacks after the wake postpone this check.
  display_reconcile_request();
}

void display_reconcile_unlock(struct bar_manager* bar_manager) {
  if (bar_manager->sleeps) {
    bar_manager_handle_system_woke(bar_manager);
    return;
  }

  display_reconcile_run(bar_manager, true);
  bar_manager_custom_events_trigger(bar_manager,
                                    COMMAND_SUBSCRIBE_SYSTEM_WOKE,
                                    NULL                          );
}
