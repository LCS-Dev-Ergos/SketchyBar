#include <assert.h>
#include <stdio.h>

#include "harness.h"
#include "mask.h"

static CFArrayRef managed_spaces;
static CFArrayRef managed_displays;
static int space_calls;
static int display_calls;

CFArrayRef SLSCopyManagedDisplaySpaces(int cid) {
  space_calls++;
  return managed_spaces ? CFRetain(managed_spaces) : NULL;
}

CFArrayRef SLSCopyManagedDisplays(int cid) {
  display_calls++;
  return managed_displays ? CFRetain(managed_displays) : NULL;
}

CFUUIDRef CGDisplayCreateUUIDFromDisplayID(CGDirectDisplayID did) {
  CFStringRef identifier = did == 1 ? CFSTR("11111111-1111-1111-1111-111111111111")
                                   : CFSTR("22222222-2222-2222-2222-222222222222");
  return CFUUIDCreateFromString(NULL, identifier);
}

uint64_t SLSManagedDisplayGetCurrentSpace(int cid, CFStringRef identifier) {
  return CFEqual(identifier, CFSTR("11111111-1111-1111-1111-111111111111")) ? 101 : 201;
}

int SLSSpaceGetType(int cid, uint64_t sid) { return 1; }

static CFDictionaryRef make_space(int64_t id) {
  CFNumberRef number = CFNumberCreate(NULL, kCFNumberSInt64Type, &id);
  const void* keys[] = { CFSTR("id64") };
  const void* values[] = { number };
  CFDictionaryRef space = CFDictionaryCreate(NULL, keys, values, 1,
                                             &kCFTypeDictionaryKeyCallBacks,
                                             &kCFTypeDictionaryValueCallBacks);
  CFRelease(number);
  return space;
}

static CFDictionaryRef make_display(CFStringRef identifier, int64_t first, int64_t second) {
  CFDictionaryRef first_space = make_space(first);
  CFDictionaryRef second_space = make_space(second);
  const void* space_values[] = { first_space, second_space };
  CFArrayRef spaces = CFArrayCreate(NULL, space_values, 2, &kCFTypeArrayCallBacks);
  const void* keys[] = { CFSTR("Display Identifier"), CFSTR("Spaces") };
  const void* values[] = { identifier, spaces };
  CFDictionaryRef display = CFDictionaryCreate(NULL, keys, values, 2,
                                               &kCFTypeDictionaryKeyCallBacks,
                                               &kCFTypeDictionaryValueCallBacks);
  CFRelease(spaces);
  CFRelease(first_space);
  CFRelease(second_space);
  return display;
}

int main(void) {
  struct bar_item items[16] = { 0 };
  struct bar_item* item_ptrs[16];
  for (int i = 0; i < 16; i++) {
    items[i].type = BAR_COMPONENT_SPACE;
    items[i].associated_space = mask_bit(i + 1);
    item_ptrs[i] = &items[i];
  }
  struct bar_manager manager = {
    .bar_items = item_ptrs,
    .bar_item_count = 16
  };

  bar_manager_update_space_components(&manager, false);
  assert(space_calls == 1);
  assert(display_calls <= 1);
  for (int i = 0; i < 16; i++)
    assert(items[i].associated_display == mask_bit(30));

  items[0].overrides_association = true;
  items[0].associated_display = mask_bit(3);
  for (int i = 1; i < 16; i++) items[i].type = BAR_ITEM;
  space_calls = 0;
  display_calls = 0;
  bar_manager_update_space_components(&manager, false);
  assert(space_calls == 0 && display_calls == 0);
  assert(items[0].associated_display == mask_bit(3));
  items[0].overrides_association = false;
  for (int i = 1; i < 16; i++) items[i].type = BAR_COMPONENT_SPACE;

  CFStringRef first_id = CFSTR("11111111-1111-1111-1111-111111111111");
  CFStringRef second_id = CFSTR("22222222-2222-2222-2222-222222222222");
  CFDictionaryRef first = make_display(first_id, 101, 102);
  CFDictionaryRef second = make_display(second_id, 201, 202);
  const void* space_values[] = { second, first };
  const void* display_values[] = { first_id, second_id };
  managed_spaces = CFArrayCreate(NULL, space_values, 2, &kCFTypeArrayCallBacks);
  managed_displays = CFArrayCreate(NULL, display_values, 2, &kCFTypeArrayCallBacks);
  space_calls = 0;
  display_calls = 0;

  bar_manager_update_space_components(&manager, false);
  assert(space_calls == 1);
  assert(display_calls == 1);
  assert(items[0].associated_display == mask_bit(2));
  assert(items[1].associated_display == mask_bit(2));
  assert(items[2].associated_display == mask_bit(1));
  assert(items[3].associated_display == mask_bit(1));
  assert(items[4].associated_display == mask_bit(30));

  CFRelease(managed_displays);
  managed_displays = NULL;
  space_calls = 0;
  display_calls = 0;
  bar_manager_update_space_components(&manager, false);
  assert(space_calls == 1 && display_calls == 1);
  assert(items[0].associated_display == mask_bit(30));

  struct bar first_bar = { .did = 1, .adid = 1, .dsid = 101, .shown = true };
  struct bar second_bar = { .did = 2, .adid = 2, .dsid = 201, .shown = true };
  struct bar* bars[] = { &first_bar, &second_bar };
  manager.bar_items = NULL;
  manager.bar_item_count = 0;
  manager.bars = bars;
  manager.bar_count = 2;
  space_calls = 0;
  display_calls = 0;
  bar_manager_handle_space_change(&manager, false);
  assert(space_calls == 1);
  assert(first_bar.sid == 3 && second_bar.sid == 1);

  CFRelease(managed_spaces);
  CFRelease(first);
  CFRelease(second);
  puts("space snapshot: passed");
  return 0;
}
