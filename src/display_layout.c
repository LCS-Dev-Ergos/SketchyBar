#include "display_layout.h"

void display_layout_read(struct display_layout* layout) {
  memset(layout, 0, sizeof(struct display_layout));
  uint32_t count = 0;
  if (CGGetActiveDisplayList(DISPLAY_LAYOUT_MAX, layout->did, &count)
      != kCGErrorSuccess) {
    return;
  }

  layout->count = count;
  for (uint32_t i = 0; i < count; i++)
    layout->bounds[i] = CGDisplayBounds(layout->did[i]);
}

bool display_session_locked(void) {
  CFDictionaryRef session = CGSessionCopyCurrentDictionary();
  if (!session) return false;

  CFTypeRef locked = CFDictionaryGetValue(session,
                                          CFSTR("CGSSessionScreenIsLocked"));
  bool result = locked
                && CFGetTypeID(locked) == CFBooleanGetTypeID()
                && CFBooleanGetValue(locked);
  CFRelease(session);
  return result;
}
