#pragma once
#include <assert.h>
#include <stdbool.h>
#include "misc/env_vars.h"

// Runs a command with sh -c, with the variables of an event added to the
// daemon's environment, without waiting for it.
bool fork_exec(char* command, struct env_vars* env_vars);

// Runs a file with sh, which also accepts a path with spaces and a file
// without an interpreter line.
bool fork_exec_file(char* path);
