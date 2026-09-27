#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "bar_manager.h"
#include "event.h"
#include "hotload.h"

char g_config_file[4096];
char g_name[256] = "sketchybar";
struct bar_manager g_bar_manager;

static int launches;
static int destroys;
static int reuse_begins;
static int reuse_ends;
static bool child_alive = true;

pid_t fork_exec_file_pid(char* path) {
  launches++;
  return getpid();
}

int kill(pid_t pid, int signal) {
  assert(pid == getpid() && signal == 0);
  if (child_alive) return 0;
  errno = ESRCH;
  return -1;
}

void bar_manager_destroy(struct bar_manager* manager) { destroys++; }
void bar_manager_init(struct bar_manager* manager) { }
void bar_manager_begin(struct bar_manager* manager) { }
void windows_reuse_begin(void) { reuse_begins++; }
void windows_reuse_end(void) { reuse_ends++; }
void event_post(struct event* event) { hotload_request(); }

static void run_for(double seconds) {
  CFAbsoluteTime end = CFAbsoluteTimeGetCurrent() + seconds;
  while (CFAbsoluteTimeGetCurrent() < end)
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.02, false);
}

static void run_until_launches(int count) {
  CFAbsoluteTime deadline = CFAbsoluteTimeGetCurrent() + 5;
  while (launches < count && CFAbsoluteTimeGetCurrent() < deadline)
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.02, false);
  assert(launches == count);
}

int main(void) {
  char directory[] = "/tmp/sketchybar-reload-gate.XXXXXX";
  assert(mkdtemp(directory));
  snprintf(g_config_file, sizeof(g_config_file), "%s/sketchybarrc", directory);
  FILE* file = fopen(g_config_file, "w");
  assert(file);
  fputs("#!/bin/sh\n", file);
  fclose(file);
  assert(chmod(g_config_file, 0700) == 0);

  hotload_request();
  hotload_request();
  hotload_request();
  run_until_launches(1);
  assert(launches == 1 && destroys == 1);
  assert(reuse_begins == 1 && reuse_ends == 0);

  hotload_request();
  run_for(0.35);
  assert(launches == 1);
  hotload_config_registered();
  hotload_config_registered();
  assert(reuse_ends == 0);
  run_until_launches(2);
  assert(launches == 2 && destroys == 2);
  assert(reuse_begins == 2 && reuse_ends == 1);

  // Short-lived shell configs do not register an event port. Their exit
  // releases the barrier and starts the queued reload.
  hotload_request();
  run_for(0.35);
  assert(launches == 2);
  child_alive = false;
  run_until_launches(3);
  assert(launches == 3 && destroys == 3);
  assert(reuse_begins == 3 && reuse_ends == 2);

  unlink(g_config_file);
  rmdir(directory);
  puts("reload gate: passed");
  return 0;
}
