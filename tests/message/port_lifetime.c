// Clones own separate send-right references, and a reload sends one stop per
// destination before releasing every reference.
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
  SEND("--add", "item", "port.owner", "left");

  mach_port_t task = mach_task_self();
  mach_port_t port;
  assert(mach_port_allocate(task, MACH_PORT_RIGHT_RECEIVE, &port) == KERN_SUCCESS);
  assert(mach_port_insert_right(task, port, port, MACH_MSG_TYPE_MAKE_SEND)
         == KERN_SUCCESS);
  item("port.owner")->event_port = port;

  SEND("--clone", "port.clone", "port.owner");
  mach_port_urefs_t refs;
  assert(mach_port_get_refs(task, port, MACH_PORT_RIGHT_SEND, &refs) == KERN_SUCCESS);
  assert(refs == 2);

  bar_manager_destroy(&g_bar_manager);
  mach_port_type_t type;
  assert(mach_port_type(task, port, &type) == KERN_SUCCESS);
  assert(!(type & MACH_PORT_TYPE_SEND));

  struct mach_buffer first = { 0 };
  assert(mach_msg(&first.message.header, MACH_RCV_MSG | MACH_RCV_TIMEOUT,
                  0, sizeof(first), port, 100, MACH_PORT_NULL) == MACH_MSG_SUCCESS);
  assert(first.message.descriptor.size == 2);
  assert(strcmp(first.message.descriptor.address, "k") == 0);
  mach_msg_destroy(&first.message.header);

  struct mach_buffer second = { 0 };
  assert(mach_msg(&second.message.header, MACH_RCV_MSG | MACH_RCV_TIMEOUT,
                  0, sizeof(second), port, 0, MACH_PORT_NULL) == MACH_RCV_TIMED_OUT);
  assert(mach_port_mod_refs(task, port, MACH_PORT_RIGHT_RECEIVE, -1) == KERN_SUCCESS);
  puts("port lifetime: passed");
  return 0;
}
