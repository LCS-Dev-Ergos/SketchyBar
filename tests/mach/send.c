// Sends events with mach_send_message (src/mach.c) to a port whose queue is
// full, as when a Lua process stops reading while it waits on the daemon.
// Each send must give up after its timeout and return the rights and memory
// of the message it could not deliver.
#include <assert.h>
#include <stdio.h>
#include <time.h>

#include "mach.h"

char g_name[256];

static uint64_t now(void) {
  return clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
}

int main(void) {
  mach_port_t task = mach_task_self();
  mach_port_t port;
  assert(mach_port_allocate(task, MACH_PORT_RIGHT_RECEIVE, &port) == KERN_SUCCESS);
  assert(mach_port_insert_right(task, port, port, MACH_MSG_TYPE_MAKE_SEND) == KERN_SUCCESS);

  struct mach_port_limits limits = { .mpl_qlimit = 1 };
  assert(mach_port_set_attributes(task, port, MACH_PORT_LIMITS_INFO,
                                  (mach_port_info_t)&limits,
                                  MACH_PORT_LIMITS_INFO_COUNT) == KERN_SUCCESS);

  // The first event fills the queue, which nobody reads.
  static char event[] = "NAME\0clock\0SENDER\0routine\0\0";
  mach_send_message(port, event, sizeof(event) - 1, false);

  mach_port_urefs_t send_refs;
  assert(mach_port_get_refs(task, port, MACH_PORT_RIGHT_SEND, &send_refs) == KERN_SUCCESS);

  // Every further event gives up instead of blocking the main thread.
  for (int i = 0; i < 5; i++) {
    uint64_t start = now();
    mach_send_message(port, event, sizeof(event) - 1, false);
    uint64_t elapsed = now() - start;
    assert(elapsed >= 50 * NSEC_PER_MSEC && elapsed < 2 * NSEC_PER_SEC);
  }

  // The send rights of the undelivered events were released.
  mach_port_urefs_t refs;
  assert(mach_port_get_refs(task, port, MACH_PORT_RIGHT_SEND, &refs) == KERN_SUCCESS);
  assert(refs == send_refs);

  mach_port_status_t status;
  mach_msg_type_number_t count = MACH_PORT_RECEIVE_STATUS_COUNT;
  assert(mach_port_get_attributes(task, port, MACH_PORT_RECEIVE_STATUS,
                                  (mach_port_info_t)&status, &count) == KERN_SUCCESS);
  assert(status.mps_msgcount == 1);

  printf("mach send timeout: passed\n");
  return 0;
}
