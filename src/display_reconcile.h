#pragma once
#include <stdbool.h>
#include <stdint.h>

// Display notifications, wake and unlock do not recreate the bars directly.
// They schedule one reconciliation after the burst of notifications, which
// compares the current displays with the layout the bars were built for and
// then keeps, refreshes or rebuilds the windows.

struct bar_manager;

// Remembers the current displays as the layout the bars are built for.
void display_reconcile_remember(void);

// Reconciles once the burst of display notifications has ended.
void display_reconcile_request(void);

// Handler of the DISPLAY_RECONCILE event.
void display_reconcile_run(struct bar_manager* bar_manager, bool unlocked);

// A system wake trusts no changed layout during a settle period.
void display_reconcile_wake(struct bar_manager* bar_manager);

// Screen unlock is not a display topology change.
void display_reconcile_unlock(struct bar_manager* bar_manager);
