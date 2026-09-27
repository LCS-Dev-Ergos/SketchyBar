#include <assert.h>
#include <stdio.h>

#include "../../src/window_reuse.c"

int g_connection = 7;
static int tracking_removals;
static int window_releases;
static int surface_destroys;

CGError SLSRemoveAllTrackingAreas(uint32_t cid, uint32_t wid) {
  assert(cid == 7 && wid != 0);
  tracking_removals++;
  return kCGErrorSuccess;
}

CGError SLSReleaseWindow(int cid, uint32_t wid) {
  assert(cid == 7 && wid != 0);
  window_releases++;
  return kCGErrorSuccess;
}

void surface_destroy(struct surface* surface) {
  assert(surface);
  surface_destroys++;
}

int main(void) {
  struct surface surface = {0};
  struct window old = {.id = 42, .surface = &surface};
  struct window next = {0};

  assert(!windows_reuse_store(&old));
  assert(!windows_reuse_take(&next));

  // Exercise the cache independent of the CI runner's macOS version.
  g_reusing_windows = true;
  assert(windows_reuse_store(&old));
  assert(tracking_removals == 1);
  assert(windows_reuse_take(&next));
  assert(next.id == old.id && next.surface == old.surface);
  assert(window_releases == 0 && surface_destroys == 0);

  assert(windows_reuse_store(&next));
  windows_reuse_end();
  assert(!windows_reuse_active());
  assert(window_releases == 1 && surface_destroys == 1);
  assert(!windows_reuse_take(&next));

  puts("window reuse: passed");
  return 0;
}
