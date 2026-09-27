// Handles commands while WindowServer answers nothing, as it does while it
// reconfigures displays or wakes: the harness has no WindowServer connection,
// so every SkyLight query fails. Display and space queries, and a bar display
// change that creates the bars again, crashed on the missing answers before.
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "harness.h"

int main(void) {
  harness_begin();

  char* response = harness_send("--add", "space", "space.1", "left", "space=1", NULL);
  free(response);

  response = harness_send("--query", "displays", NULL);
  assert(response);
  free(response);

  response = harness_send("--query", "bar", NULL);
  assert(strstr(response, "\"position\""));
  free(response);

  // Display indices a 32-bit mask cannot hold select no display.
  free(harness_send("--bar", "display=0,40,2", NULL));
  assert(g_bar_manager.displays == 0x2);

  free(harness_send("--bar", "display=all", NULL));
  assert(g_bar_manager.displays == DISPLAY_ALL_PATTERN);

  printf("without WindowServer: passed\n");
  return 0;
}
