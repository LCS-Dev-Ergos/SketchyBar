// Runs scripts through src/script.c in a temporary directory: the event
// variables reach the script but not the daemon's environment, configuration
// paths may contain spaces, and the alarm ends a script unless it cancels it.
// The build shortens the alarm to one second.
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "script.h"

static char directory[] = "/tmp/sketchybar-script-tests.XXXXXX";

static char* path(const char* name) {
  static char buffer[4][256];
  static int next = 0;
  char* result = buffer[next++ % 4];
  snprintf(result, 256, "%s/%s", directory, name);
  return result;
}

// Waits up to five seconds for a script to write the file, then returns its
// contents. Scripts write elsewhere first and rename, so a file is complete.
static const char* wait_for(const char* name) {
  static char contents[256];
  for (int i = 0; i < 500; i++) {
    FILE* file = fopen(path(name), "r");
    if (file) {
      size_t length = fread(contents, 1, sizeof(contents) - 1, file);
      contents[length] = '\0';
      fclose(file);
      return contents;
    }
    usleep(10000);
  }
  return NULL;
}

static void write_script(const char* name, const char* text) {
  FILE* file = fopen(path(name), "w");
  assert(file);
  fputs(text, file);
  fclose(file);
  assert(chmod(path(name), 0700) == 0);
}

int main(void) {
  // The daemon ignores SIGCHLD, so its scripts are reaped without waiting.
  signal(SIGCHLD, SIG_IGN);
  assert(mkdtemp(directory));
  setenv("RESULTS", directory, 1);

  // The event variables replace or extend the environment of the script only.
  setenv("SKETCHYBAR_TEST_SHARED", "daemon", 1);
  struct env_vars env_vars;
  env_vars_init(&env_vars);
  env_vars_set(&env_vars, strdup("SKETCHYBAR_TEST_SHARED"), strdup("event"));
  env_vars_set(&env_vars, strdup("SKETCHYBAR_TEST_EVENT"), strdup("only"));
  env_vars_set(&env_vars, strdup("SKETCHYBAR=TEST"), strdup("refused"));
  assert(fork_exec("printf '%s %s %s' \"$SKETCHYBAR_TEST_SHARED\" \"$SKETCHYBAR_TEST_EVENT\""
                   " \"${HOME:+home}\" > \"$RESULTS/env.tmp\" && mv \"$RESULTS/env.tmp\" \"$RESULTS/env\"",
                   &env_vars));
  const char* seen = wait_for("env");
  assert(seen && strcmp(seen, "event only home") == 0);
  assert(strcmp(getenv("SKETCHYBAR_TEST_SHARED"), "daemon") == 0);
  assert(!getenv("SKETCHYBAR_TEST_EVENT"));
  env_vars_destroy(&env_vars);

  // A configuration in a directory with spaces runs, with or without an
  // interpreter line.
  assert(mkdir(path("config dir"), 0700) == 0);
  write_script("config dir/sketchybarrc",
               "#!/bin/sh\nprintf ok > \"$RESULTS/rc.tmp\" && mv \"$RESULTS/rc.tmp\" \"$RESULTS/rc\"\n");
  assert(fork_exec_file(path("config dir/sketchybarrc")));
  seen = wait_for("rc");
  assert(seen && strcmp(seen, "ok") == 0);

  write_script("config dir/plain",
               "printf ok > \"$RESULTS/plain.tmp\" && mv \"$RESULTS/plain.tmp\" \"$RESULTS/plain\"\n");
  assert(fork_exec_file(path("config dir/plain")));
  seen = wait_for("plain");
  assert(seen && strcmp(seen, "ok") == 0);

  // The alarm ends a script that outlives it, and only the script: this
  // process would end as well if the alarm were its own. A script that
  // cancels the alarm, as long-running helpers do, keeps running.
  assert(fork_exec("sleep 3; printf late > \"$RESULTS/late\"", NULL));
  assert(fork_exec("exec /usr/bin/perl -e 'alarm 0; sleep 2; open F, \">\", \"$ARGV[0].tmp\" or die;"
                   " print F \"kept\"; close F; rename \"$ARGV[0].tmp\", $ARGV[0]' \"$RESULTS/kept\"",
                   NULL));
  seen = wait_for("kept");
  assert(seen && strcmp(seen, "kept") == 0);
  sleep(2);
  assert(access(path("late"), F_OK) != 0);

  char command[512];
  snprintf(command, sizeof(command), "rm -rf '%s'", directory);
  system(command);

  printf("scripts: passed\n");
  return 0;
}
