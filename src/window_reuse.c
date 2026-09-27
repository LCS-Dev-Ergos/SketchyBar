#include "window_reuse.h"
#include "window.h"
#include "misc/helpers.h"

struct reused_window {
  struct reused_window* next;
  uint32_t id;
  CGContextRef context;
  struct surface* surface;
};

static struct reused_window* g_reused_windows;
static bool g_reusing_windows;

void windows_reuse_begin(void) {
  if (__builtin_available(macOS 26.0, *))
    g_reusing_windows = true;
}

bool windows_reuse_active(void) {
  return g_reusing_windows;
}

bool windows_reuse_take(struct window* window) {
  if (!g_reusing_windows || !g_reused_windows) return false;

  struct reused_window* reused = g_reused_windows;
  g_reused_windows = reused->next;
  window->id = reused->id;
  window->context = reused->context;
  window->surface = reused->surface;
  free(reused);
  return true;
}

bool windows_reuse_store(struct window* window) {
  if (!g_reusing_windows || !window->id || !window->surface) return false;

  struct reused_window* reused = malloc(sizeof(struct reused_window));
  if (!reused) return false;

  SLSRemoveAllTrackingAreas(g_connection, window->id);
  *reused = (struct reused_window) {
    .next = g_reused_windows,
    .id = window->id,
    .context = window->context,
    .surface = window->surface,
  };
  g_reused_windows = reused;
  return true;
}

void windows_reuse_end(void) {
  g_reusing_windows = false;
  while (g_reused_windows) {
    struct reused_window* reused = g_reused_windows;
    g_reused_windows = reused->next;
    surface_destroy(reused->surface);
    if (reused->context) CGContextRelease(reused->context);
    SLSReleaseWindow(g_connection, reused->id);
    free(reused);
  }
}
