#include "bar_manager.h"
#include "event.h"
#include "hotload.h"
#include "script.h"
#include <ApplicationServices/ApplicationServices.h>
#include <dispatch/dispatch.h>
#include <errno.h>
#include <libgen.h>
#include <signal.h>

extern char g_config_file[4096];
extern char g_name[256];
bool g_hotload = false;
int64_t g_last_hotload = 0;

#ifndef RELOAD_QUIET_NS
#define RELOAD_QUIET_NS (200 * NSEC_PER_MSEC)
#endif

static uint64_t g_reload_generation;
static uint64_t g_config_generation;
static bool g_configuring;
static bool g_reload_pending;
static pid_t g_config_pid;

static void config_finished(void) {
  if (!g_configuring) return;
  g_configuring = false;
  g_config_pid = 0;
  g_config_generation++;
  if (g_reload_pending) {
    g_reload_pending = false;
    hotload_request();
  }
}

// A config without a Mach event port can finish normally. Do not hold its
// reload barrier after the direct child has exited.
static void watch_config_pid(uint64_t generation) {
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 100 * NSEC_PER_MSEC),
                 dispatch_get_main_queue(), ^{
    if (generation != g_config_generation || !g_configuring) return;
    if (kill(g_config_pid, 0) == -1 && errno == ESRCH) {
      config_finished();
      return;
    }
    watch_config_pid(generation);
  });
}

void hotload_set_state(int state) {
  g_hotload = state;
}

int hotload_get_state() {
  return g_hotload;
}

bool set_config_file_path(char* file) {
  char* path = realpath(file, NULL);
  if (path) {
    snprintf(g_config_file, sizeof(g_config_file), "%s", path);
    free(path);
    return true;
  }
  return false;
}

static bool get_config_file(char *restrict filename, char *restrict buffer, int buffer_size) {
  char *xdg_home = getenv("XDG_CONFIG_HOME");
  if (xdg_home && *xdg_home) {
    snprintf(buffer, buffer_size, "%s/%s/%s", xdg_home, g_name, filename);
    if (file_exists(buffer)) return true;
  }

  char *home = getenv("HOME");
  if (!home) return false;

  snprintf(buffer, buffer_size, "%s/.config/%s/%s", home, g_name, filename);
  if (file_exists(buffer)) return true;

  snprintf(buffer, buffer_size, "%s/.%s", home, filename);
  return file_exists(buffer);
}

void exec_config_file() {
  g_configuring = true;
  if (!*g_config_file
    && !get_config_file("sketchybarrc", g_config_file, sizeof(g_config_file))) {
    printf("could not locate config file..\n");
    config_finished();
    return;
  }

  if (!file_exists(g_config_file)) {
    printf("file '%s' does not exist..\n", g_config_file);
    config_finished();
    return;
  }

  setenv("CONFIG_DIR", dirname(g_config_file), 1);
  chdir(dirname(g_config_file));

  if (!ensure_executable_permission(g_config_file)) {
    printf("could not set the executable permission bit for '%s'\n", g_config_file);
    config_finished();
    return;
  }

  g_config_pid = fork_exec_file_pid(g_config_file);
  if (!g_config_pid) {
    printf("failed to execute file '%s'\n", g_config_file);
    config_finished();
    return;
  }
  watch_config_pid(++g_config_generation);
}

static void reload_now(void) {
  bar_manager_destroy(&g_bar_manager);
  bar_manager_init(&g_bar_manager);
  bar_manager_begin(&g_bar_manager);
  exec_config_file();
}

void hotload_request(void) {
  uint64_t generation = ++g_reload_generation;
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, RELOAD_QUIET_NS),
                 dispatch_get_main_queue(), ^{
    if (generation != g_reload_generation) return;
    if (g_configuring) {
      g_reload_pending = true;
      return;
    }
    reload_now();
  });
}

void hotload_config_registered(void) {
  config_finished();
}

static void handler(ConstFSEventStreamRef stream, void* context, size_t count, void* paths, const FSEventStreamEventFlags* flags, const FSEventStreamEventId* ids) {
  if (g_hotload && count > 0) {
    // Limit the hotload rate to avoid locking up the system on a hotload loop
    int64_t time = clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW_APPROX);
    if (time - g_last_hotload > (1ULL << 30)) {
      g_last_hotload = time;
      struct event event = { NULL, HOTLOAD };
      event_post(&event);
    }
  }
}

int begin_receiving_config_change_events() {
  char* file = dirname(g_config_file);
  CFStringRef file_ref = CFStringCreateWithCString(
                             kCFAllocatorDefault, file, kCFStringEncodingUTF8);

  CFArrayRef paths = CFArrayCreate(NULL,
                                   (const void**)&file_ref,
                                   1,
                                   &kCFTypeArrayCallBacks);

  FSEventStreamRef stream = FSEventStreamCreate(
                                         kCFAllocatorDefault,
                                         handler,
                                         NULL,
                                         paths,
                                         kFSEventStreamEventIdSinceNow,
                                         0.5,
                                         kFSEventStreamCreateFlagNoDefer
                                         | kFSEventStreamCreateFlagFileEvents);

  CFRelease(file_ref);
  CFRelease(paths);

  FSEventStreamScheduleWithRunLoop(stream, CFRunLoopGetCurrent(),
                                           kCFRunLoopDefaultMode);

  FSEventStreamStart(stream);
  return 0;
}
