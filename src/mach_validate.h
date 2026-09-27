#pragma once
#include "mach.h"
#include <string.h>

// Any process in the session can send to the bootstrap port, so a message is
// checked before its descriptor is read. A command carries its tokens in one
// out-of-line descriptor, each token terminated by a NUL and the command by an
// empty token. SbarLua and the CLI send it, but older native event providers
// end at the last argument's NUL. The receiver appends the missing token before
// parsing those messages.
static inline bool mach_message_has_empty_token(const struct mach_message* message) {
  return memmem(message->descriptor.address,
                message->descriptor.size,
                "\0\0",
                2                          ) != NULL;
}

static inline bool mach_message_valid(const struct mach_message* message, size_t size) {
  if (size < sizeof(struct mach_message)) return false;
  if (!(message->header.msgh_bits & MACH_MSGH_BITS_COMPLEX)) return false;
  if (message->msgh_descriptor_count != 1) return false;
  if (message->descriptor.type != MACH_MSG_OOL_DESCRIPTOR) return false;
  if (!message->descriptor.address) return false;
  if (!message->descriptor.size) return false;

  const char* data = message->descriptor.address;
  return mach_message_has_empty_token(message)
         || data[message->descriptor.size - 1] == '\0';
}
