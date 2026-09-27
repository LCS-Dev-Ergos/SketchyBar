// libFuzzer target for the tokenizer every command of the message protocol
// goes through (src/misc/helpers.h): tokens, key-value pairs, comma separated
// lists, numbers and boolean states, exactly as message.c and bar_item.c use
// them. The input is framed like a client message: every argument ends with a
// NUL and the message with a second one (see client_send_message).
#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "misc/helpers.h"

// Mirrors reformat_batch_key_value_pair in message.c.
static void fuzz_key_value_pair(struct token token) {
  char* copy = token_to_string(token);
  struct key_value_pair key_value_pair = get_key_value_pair(copy, '=');

  if (key_value_pair.key) {
    size_t length = strlen(key_value_pair.key)
                    + (key_value_pair.value ? strlen(key_value_pair.value) : 0)
                    + 3;

    char* packed = malloc(length);
    pack_key_value_pair(packed, &key_value_pair);

    char* cursor = packed;
    struct token key = get_token(&cursor);
    struct token value = get_token(&cursor);
    assert(key.length == strlen(key_value_pair.key));
    assert(value.length == (key_value_pair.value
                            ? strlen(key_value_pair.value)
                            : 0));
    free(packed);
  }

  free(copy);
}

// Mirrors the space, display and bar display lists.
static void fuzz_list(struct token token) {
  char* copy = token_to_string(token);
  struct token list_token = { copy, token.length };

  uint32_t count = 0;
  char** list = token_split(list_token, ',', &count);
  if (list) {
    for (uint32_t i = 0; i < count; i++) {
      assert(list[i] >= copy && list[i] <= copy + token.length);
    }
    free(list);
  }

  free(copy);
}

static void fuzz_token(struct token token) {
  token_to_int(token);
  token_to_uint32t(token);
  token_to_float(token);
  evaluate_boolean_state(token, false);

  char* string = token_to_string(token);
  assert(strlen(string) == token.length);
  free(string);

  fuzz_key_value_pair(token);
  fuzz_list(token);
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  char* message = malloc(size + 2);
  memcpy(message, data, size);
  message[size] = '\0';
  message[size + 1] = '\0';

  char* cursor = message;
  struct token token = get_token(&cursor);
  while (token.text && token.length > 0) {
    fuzz_token(token);
    token = get_token(&cursor);
  }

  free(message);
  return 0;
}
