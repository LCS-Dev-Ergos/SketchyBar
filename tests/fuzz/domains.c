// Exercises add, set and query through the daemon's message handler without
// starting scripts, resolving live Mach services or creating bar windows.
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness.h"
#include "mach.h"
#include "script.h"

bool fork_exec(char* command, struct env_vars* env_vars) {
  (void)command;
  (void)env_vars;
  return false;
}

bool fork_exec_file(char* path) {
  (void)path;
  return false;
}

pid_t fork_exec_file_pid(char* path) {
  (void)path;
  return 0;
}

mach_port_t mach_get_bs_port(char* bs_name) {
  (void)bs_name;
  return MACH_PORT_NULL;
}

int LLVMFuzzerInitialize(int* argc, char*** argv) {
  (void)argc;
  (void)argv;
  assert(freopen("/dev/null", "w", stdout));
  return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 2 || size > 514) return 0;

  // An argument cannot introduce a second domain (notably --exit). Keep the
  // input printable and NUL-free so the entire suffix reaches the handler.
  char value[513];
  for (size_t i = 2; i < size; i++) {
    uint8_t c = data[i] & 0x7f;
    value[i - 2] = c < 32 || c == 127 || c == '-' ? '_' : (char)c;
  }
  value[size - 2] = '\0';

  harness_begin();
  assert(g_bar_manager.bar_count == 0);
  free(harness_send("--add", "item", "fuzz", "left", NULL));

  char* response;
  switch (data[0] % 3) {
    case 0: {
      const char* positions[] = { "left", "right", "center" };
      response = harness_send("--add", "item", value,
                              positions[data[1] % 3], NULL);
      break;
    }
    case 1: {
      const char* keys[] = { "label", "icon", "width", "space",
                             "display", "script", "click_script", "mach_helper" };
      const char* key = keys[data[1] % 8];
      size_t length = strlen(key) + strlen(value) + 2;
      char* pair = malloc(length);
      assert(pair);
      snprintf(pair, length, "%s=%s", key, value);
      response = harness_send("--set", "fuzz", pair, NULL);
      free(pair);
      break;
    }
    default: {
      const char* queries[] = { "item", "bar", "defaults", "events" };
      const char* query = queries[data[1] % 4];
      response = strcmp(query, "item") == 0
                   ? harness_send("--query", query, value, NULL)
                   : harness_send("--query", query, NULL);
      break;
    }
  }

  free(response);
  bar_manager_destroy(&g_bar_manager);
  return 0;
}
