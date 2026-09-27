// Sends client and malformed messages to the receive callback of src/mach.c,
// set up on a private port as mach_server_begin sets it up on the bootstrap
// port, and checks which of them reach the handler.
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <CoreFoundation/CoreFoundation.h>

#include "mach.h"
#include "mach_validate.h"

char g_name[256];
void mach_message_callback(CFMachPortRef port, void* message, CFIndex size, void* context);

static int handled;
static char command[64];

static MACH_HANDLER(record) {
  handled++;
  strlcpy(command, message->message.descriptor.address, sizeof(command));
}

static void run_until_handled(int count) {
  for (int i = 0; i < 50 && handled < count; i++) {
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, false);
  }
  CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.05, false);
}

// Sends a message as a client would, with the given header bits, descriptor
// count and descriptor type.
static void send_raw(mach_port_t port, mach_msg_bits_t complex, uint32_t count, uint32_t type,
                     const char* data, uint32_t size) {
  struct mach_message message = { 0 };
  message.header.msgh_remote_port = port;
  message.header.msgh_bits = MACH_MSGH_BITS_SET(MACH_MSG_TYPE_COPY_SEND, 0, 0, complex);
  message.header.msgh_size = sizeof(message);
  message.msgh_descriptor_count = count;
  message.descriptor.address = (void*)data;
  message.descriptor.size = size;
  message.descriptor.copy = MACH_MSG_VIRTUAL_COPY;
  message.descriptor.type = type;

  kern_return_t result = mach_msg(&message.header, MACH_SEND_MSG, sizeof(message),
                                  0, MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
  assert(result == MACH_MSG_SUCCESS);
}

int main(void) {
  // The first pair of NULs must lie within the descriptor, and the message
  // within the received size.
  struct mach_message message = { .header.msgh_bits = MACH_MSGH_BITS_COMPLEX,
                                  .msgh_descriptor_count = 1               };
  message.descriptor.type = MACH_MSG_OOL_DESCRIPTOR;
  message.descriptor.address = "bar\0\0";
  message.descriptor.size = 5;
  assert(mach_message_valid(&message, sizeof(message)));
  assert(!mach_message_valid(&message, sizeof(message) - 1));
  message.descriptor.size = 4;
  assert(!mach_message_valid(&message, sizeof(message)));
  message.descriptor.size = 0;
  assert(!mach_message_valid(&message, sizeof(message)));

  mach_port_t task = mach_task_self();
  mach_port_t port;
  assert(mach_port_allocate(task, MACH_PORT_RIGHT_RECEIVE, &port) == KERN_SUCCESS);
  assert(mach_port_insert_right(task, port, port, MACH_MSG_TYPE_MAKE_SEND) == KERN_SUCCESS);

  struct mach_server server = { .handler = record };
  CFMachPortContext context = { 0, &server };
  CFMachPortRef cf_port = CFMachPortCreateWithPort(NULL, port, mach_message_callback, &context, NULL);
  CFRunLoopSourceRef source = CFMachPortCreateRunLoopSource(NULL, cf_port, 0);
  CFRunLoopAddSource(CFRunLoopGetMain(), source, kCFRunLoopDefaultMode);

  // A command framed as the client frames it reaches the handler, including
  // the bytes the client sends after the terminating empty token.
  static char query[] = "--query\0bar\0\0\x7f\x7f";
  mach_send_message(port, query, sizeof(query) - 1, false);
  run_until_handled(1);
  assert(handled == 1 && strcmp(command, "--query") == 0);

  // Without the complex bit the kernel copies the descriptor inline, and the
  // sender chooses its address.
  send_raw(port, 0, 1, MACH_MSG_OOL_DESCRIPTOR, (const char*)16, 64);
  run_until_handled(2);
  assert(handled == 1);

  // A command without its terminating empty token would be read past its end.
  static char unterminated[] = "--query\0bar";
  mach_send_message(port, unterminated, sizeof(unterminated) - 1, false);
  run_until_handled(2);
  assert(handled == 1);

  // Another descriptor count or type is not a command.
  send_raw(port, MACH_MSGH_BITS_COMPLEX, 0, MACH_MSG_OOL_DESCRIPTOR, NULL, 0);
  run_until_handled(2);
  assert(handled == 1);

  // The next well-formed command is still handled.
  static char set[] = "--set\0clock\0label=12:00\0\0";
  mach_send_message(port, set, sizeof(set) - 1, false);
  run_until_handled(2);
  assert(handled == 2 && strcmp(command, "--set") == 0);

  CFRelease(source);
  CFRelease(cf_port);
  printf("mach server: passed\n");
  return 0;
}
