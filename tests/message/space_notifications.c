#include <assert.h>
#include <stdio.h>

#include "event.h"
#include "harness.h"

extern int g_space_management_mode;

static int space_calls;

CFArrayRef SLSCopyManagedDisplaySpaces(int cid) {
  space_calls++;
  return NULL;
}

static void wait_for_update(int expected) {
  CFAbsoluteTime deadline = CFAbsoluteTimeGetCurrent() + 3;
  while (space_calls < expected && CFAbsoluteTimeGetCurrent() < deadline)
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.02, true);
  assert(space_calls == expected);
}

int main(void) {
  // No bar or WindowServer connection is created by the message harness.
  g_space_management_mode = 1;
  struct event event = { NULL, SPACE_CHANGED };

  event_post(&event);
  event_post(&event);
  event_post(&event);
  wait_for_update(1);
  CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.05, false);
  assert(space_calls == 1);

  event_post(&event);
  wait_for_update(2);
  bar_manager_handle_space_change(&g_bar_manager, true);
  assert(space_calls == 3);
  puts("space notifications: passed");
  return 0;
}
