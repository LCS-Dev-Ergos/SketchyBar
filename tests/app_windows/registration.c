#include <assert.h>
#include <stdio.h>

#include "app_windows.h"
#include "../../src/app_windows.c"

int g_connection;
pid_t g_pid;
static int registrations;
static uint32_t available_displays = 1;

uint32_t* display_active_display_list(uint32_t* count) {
  *count = available_displays;
  if (!available_displays) return NULL;
  uint32_t* displays = malloc(sizeof(uint32_t));
  *displays = 1;
  return displays;
}

uint64_t* display_space_list(uint32_t did, int* count) {
  assert(did == 1);
  *count = 11;
  uint64_t* spaces = malloc(sizeof(uint64_t) * *count);
  for (int i = 0; i < *count; i++) spaces[i] = i + 1;
  return spaces;
}

CFArrayRef SLSCopyWindowsWithOptionsAndTags(int cid,
                                             uint32_t owner,
                                             CFArrayRef spaces,
                                             uint32_t options,
                                             uint64_t* set_tags,
                                             uint64_t* clear_tags) {
  return NULL;
}

CGError SLSRequestNotificationsForWindows(int cid,
                                           uint32_t* windows,
                                           uint32_t count) {
  registrations++;
  assert(count == 1);
  assert(windows[0] == 42);
  return kCGErrorSuccess;
}

char* workspace_copy_app_name_for_pid(pid_t pid) {
  return NULL;
}

void event_post(struct event* event) { }

int main(void) {
  // A full scan must replace the notification list once, after every space
  // has contributed its windows. The previous implementation did it 11 times.
  struct app_window known = { .wid = 42, .sid = 99, .pid = 1 };
  g_windows.windows = &known;
  g_windows.num_windows = 1;
  update_all_spaces(&g_windows, true);
  assert(registrations == 1);

  available_displays = 0;
  update_all_spaces(&g_windows, true);
  assert(registrations == 1);

  app_windows_update_space(&g_windows, 12, true, true);
  assert(registrations == 2);
  puts("app window registration: passed");
  return 0;
}
