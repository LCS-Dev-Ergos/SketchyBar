// libFuzzer target for the checks a received Mach message passes before the
// server reads its descriptor (src/mach_validate.h). The first input byte
// selects the complex bit, the descriptor count and the descriptor type; the
// rest is the descriptor, in a buffer of exactly its size. Every accepted
// message must tokenize without reading past the descriptor.
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mach_validate.h"
#include "misc/helpers.h"

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 1) return 0;

  const uint32_t types[] = { MACH_MSG_PORT_DESCRIPTOR,
                             MACH_MSG_OOL_DESCRIPTOR,
                             MACH_MSG_OOL_PORTS_DESCRIPTOR,
                             MACH_MSG_OOL_VOLATILE_DESCRIPTOR };

  struct mach_message message = { 0 };
  message.header.msgh_bits = (data[0] & 1) ? MACH_MSGH_BITS_COMPLEX : 0;
  message.msgh_descriptor_count = (data[0] >> 1) & 3;
  message.descriptor.type = types[(data[0] >> 3) & 3];

  size_t length = size - 1;
  char* descriptor = malloc(length ? length : 1);
  memcpy(descriptor, data + 1, length);
  message.descriptor.address = descriptor;
  message.descriptor.size = length;

  if (mach_message_valid(&message, sizeof(message))) {
    char* cursor = descriptor;
    struct token token = get_token(&cursor);
    while (token.text && token.length > 0) {
      assert(token.text >= descriptor);
      assert(token.text + token.length < descriptor + length);
      token = get_token(&cursor);
    }
  }

  free(descriptor);
  return 0;
}
