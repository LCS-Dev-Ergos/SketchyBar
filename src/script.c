#include "script.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Seconds a script runs before SIGALRM ends it (upstream #71). A long-running
// helper, such as SbarLua's event loop, cancels the alarm with alarm(0), so
// the child must set it before exec rather than the daemon keep a timer.
#ifndef FORK_TIMEOUT
#define FORK_TIMEOUT 60
#endif

extern char** environ;

static bool environment_overrides(struct env_vars* env_vars, const char* entry) {
  if (!env_vars) return false;

  for (int i = 0; i < env_vars->count; i++) {
    size_t length = strlen(env_vars->vars[i]->key);
    if (strncmp(entry, env_vars->vars[i]->key, length) == 0
        && entry[length] == '=') {
      return true;
    }
  }
  return false;
}

// The daemon's environment with the variables of the event in place of those
// of the same name. It is built before vfork, so that the daemon's own
// environment never changes and the child only sets its alarm and execs.
static char** environment_create(struct env_vars* env_vars) {
  size_t count = 0;
  while (environ[count]) count++;
  if (env_vars) count += env_vars->count;

  char** environment = malloc((count + 1) * sizeof(char*));
  if (!environment) return NULL;

  size_t index = 0;
  for (char** entry = environ; *entry; entry++) {
    if (environment_overrides(env_vars, *entry)) continue;
    environment[index++] = strdup(*entry);
  }

  for (int i = 0; env_vars && i < env_vars->count; i++) {
    char* key = env_vars->vars[i]->key;
    // setenv refuses these names, and so did the daemon before.
    if (!*key || strchr(key, '=')) continue;

    char* value = env_vars->vars[i]->value ? env_vars->vars[i]->value : "";
    size_t size = strlen(key) + strlen(value) + 2;
    environment[index] = malloc(size);
    snprintf(environment[index++], size, "%s=%s", key, value);
  }

  environment[index] = NULL;
  return environment;
}

static void environment_destroy(char** environment) {
  for (char** entry = environment; *entry; entry++) free(*entry);
  free(environment);
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
static pid_t spawn(char** arguments, char** environment) {
  pid_t pid = vfork();
  if (pid == -1) return 0;
  if (pid != 0) return pid;

  alarm(FORK_TIMEOUT);
  execve(arguments[0], arguments, environment);
  _exit(127);
}
#pragma clang diagnostic pop

bool fork_exec(char* command, struct env_vars* env_vars) {
  char** environment = environment_create(env_vars);
  if (!environment) return false;

  char* arguments[] = { "/usr/bin/env", "sh", "-c", command, NULL };
  bool spawned = spawn(arguments, environment);
  environment_destroy(environment);
  return spawned;
}

pid_t fork_exec_file_pid(char* path) {
  char** environment = environment_create(NULL);
  if (!environment) return 0;

  char* arguments[] = { "/usr/bin/env", "sh", "-c", "\"$0\"", path, NULL };
  pid_t pid = spawn(arguments, environment);
  environment_destroy(environment);
  return pid;
}

bool fork_exec_file(char* path) {
  return fork_exec_file_pid(path) != 0;
}
