#pragma once
#include "mach.h"
#include <string.h>

// Any process in the session can send to the bootstrap port, so a message is
// checked before its descriptor is read. A command carries its tokens in one
// out-of-line descriptor, each token terminated by a NUL and the command by an
// empty token. The tokenizer never reads past the first pair of NULs, which
// must lie within the descriptor; the client may send bytes after it.
static inline bool mach_message_valid(const struct mach_message* message, size_t size) {
  if (size < sizeof(struct mach_message)) return false;
  if (!(message->header.msgh_bits & MACH_MSGH_BITS_COMPLEX)) return false;
  if (message->msgh_descriptor_count != 1) return false;
  if (message->descriptor.type != MACH_MSG_OOL_DESCRIPTOR) return false;
  if (!message->descriptor.address) return false;

  return memmem(message->descriptor.address,
                message->descriptor.size,
                "\0\0",
                2                          ) != NULL;
}
