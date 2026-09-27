#pragma once

#include <stdbool.h>

struct window;

void windows_reuse_begin(void);
void windows_reuse_end(void);
bool windows_reuse_active(void);
bool windows_reuse_take(struct window* window);
bool windows_reuse_store(struct window* window);
