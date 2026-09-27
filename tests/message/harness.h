#pragma once
// Runs the daemon's message handlers in a test process, linked against every
// source but src/sketchybar.c. No bar is created, so items never get windows
// and nothing appears on screen.
#include "bar_manager.h"
#include "message.h"

// Initialises the bar manager as the daemon does, without bars, displays or
// WindowServer notifications.
void harness_begin(void);

// Handles one command, given as its NULL-terminated arguments, as the daemon
// handles a client message, and returns the response, which the caller frees.
char* harness_send(const char* argument, ...);
