#include "harness.h"
#include <stdarg.h>

// The globals src/sketchybar.c defines.
CGError (* SBSLSTransactionAddPostDecodeAction)(CFTypeRef, void (^)()) = NULL;
int g_connection;
CFTypeRef g_transaction;
int g_space_management_mode;
struct bar_manager g_bar_manager;
struct mach_server g_mach_server;
void* g_workspace_context;
char g_name[256] = "sketchybar-tests";
char g_config_file[4096];
char g_lock_file[MAXLEN];
bool g_volume_events;
bool g_brightness_events;
int64_t g_disable_capture = 0;
pid_t g_pid = 0;

void harness_begin(void) {
  signal(SIGCHLD, SIG_IGN);
  signal(SIGPIPE, SIG_IGN);
  bar_manager_init(&g_bar_manager);
}

char* harness_send(const char* argument, ...) {
  // Frames the arguments as client_send_message does.
  size_t length = 1;
  va_list arguments;
  va_start(arguments, argument);
  for (const char* next = argument; next; next = va_arg(arguments, const char*)) {
    length += strlen(next) + 1;
  }
  va_end(arguments);

  char* message = malloc(length);
  char* cursor = message;
  va_start(arguments, argument);
  for (const char* next = argument; next; next = va_arg(arguments, const char*)) {
    size_t size = strlen(next) + 1;
    memcpy(cursor, next, size);
    cursor += size;
  }
  va_end(arguments);
  *cursor = '\0';

  // The response goes to a port of this process, as to a waiting client.
  mach_port_t task = mach_task_self();
  mach_port_t port;
  assert(mach_port_allocate(task, MACH_PORT_RIGHT_RECEIVE, &port) == KERN_SUCCESS);
  assert(mach_port_insert_right(task, port, port, MACH_MSG_TYPE_MAKE_SEND) == KERN_SUCCESS);

  struct mach_buffer buffer = { 0 };
  buffer.message.header.msgh_remote_port = port;
  buffer.message.descriptor.address = message;
  buffer.message.descriptor.size = length;
  handle_message_mach(&buffer);
  free(message);

  struct mach_buffer reply = { 0 };
  kern_return_t result = mach_msg(&reply.message.header, MACH_RCV_MSG | MACH_RCV_TIMEOUT,
                                  0, sizeof(reply), port, 0, MACH_PORT_NULL);
  char* response = strdup(result == MACH_MSG_SUCCESS && reply.message.descriptor.address
                          ? reply.message.descriptor.address
                          : "");
  if (result == MACH_MSG_SUCCESS) mach_msg_destroy(&reply.message.header);

  mach_port_mod_refs(task, port, MACH_PORT_RIGHT_RECEIVE, -1);
  mach_port_deallocate(task, port);
  return response;
}
