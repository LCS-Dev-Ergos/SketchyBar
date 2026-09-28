#include "native_menu_windows.h"
#include <unistd.h>

// Query only after the cursor has left the menu strip for the exit delay.
// Window dictionaries are too expensive to enumerate on every mouse poll.
bool native_menu_popup_visible(CGRect display_bounds) {
  CFArrayRef windows = CGWindowListCopyWindowInfo(
    kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements,
    kCGNullWindowID);
  if (!windows) return false;

  bool found = false;
  for (CFIndex i = 0; i < CFArrayGetCount(windows); i++) {
    CFDictionaryRef window = CFArrayGetValueAtIndex(windows, i);
    CFNumberRef layer = CFDictionaryGetValue(window, kCGWindowLayer);
    CFNumberRef owner = CFDictionaryGetValue(window, kCGWindowOwnerPID);
    CFDictionaryRef bounds = CFDictionaryGetValue(window, kCGWindowBounds);
    int level = 0;
    int pid = 0;
    CGRect frame;
    if (!layer || !owner || !bounds
        || !CFNumberGetValue(layer, kCFNumberIntType, &level)
        || !CFNumberGetValue(owner, kCFNumberIntType, &pid)
        || !CGRectMakeWithDictionaryRepresentation(bounds, &frame)) continue;

    if (level == kCGPopUpMenuWindowLevel && pid != getpid()
        && CGRectIntersectsRect(frame, display_bounds)
        && CGRectGetMinY(frame) <= CGRectGetMinY(display_bounds) + 60) {
      found = true;
      break;
    }
  }

  CFRelease(windows);
  return found;
}
