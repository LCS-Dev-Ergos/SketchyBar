#pragma once
#include <stdint.h>

// The bit of an index in a 32-bit mask of spaces, displays or bars. An index
// the mask cannot hold, such as a user value of 32 or more or a display of 0
// less one, selects no bit instead of an undefined shift.
static inline uint32_t mask_bit(unsigned long index) {
  return index < 32 ? (uint32_t)1 << index : 0;
}
