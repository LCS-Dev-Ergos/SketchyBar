// Space and display masks have 32 bits. Out-of-range indices must select no
// item or bar, and bit 31 must work without a signed shift.
#include <assert.h>
#include <stdio.h>

#include "harness.h"
#include "mask.h"

bool bar_manager_bar_needs_redraw(struct bar_manager* manager, struct bar* bar);

int main(void) {
  struct bar bar = { .shown = true, .adid = 1, .sid = 1 };
  struct bar_item item = {
    .drawing = true,
    .type = BAR_ITEM,
    .associated_display = mask_bit(1),
    .associated_space = mask_bit(1)
  };

  assert(bar_draws_item(&bar, &item));

  bar.sid = 31;
  item.associated_space = mask_bit(31);
  assert(bar_draws_item(&bar, &item));
  bar.sid = 32;
  assert(!bar_draws_item(&bar, &item));

  bar.sid = 1;
  item.associated_space = mask_bit(1);
  bar.adid = 31;
  item.associated_display = mask_bit(31);
  assert(bar_draws_item(&bar, &item));
  bar.adid = 32;
  assert(!bar_draws_item(&bar, &item));

  struct bar_item* items[] = { &item };
  struct bar* bars[] = { &bar };
  struct bar_manager manager = {
    .bar_items = items,
    .bar_item_count = 1,
    .bars = bars,
    .bar_count = 1
  };
  assert(!bar_manager_bar_needs_redraw(&manager, &bar));

  item.type = BAR_COMPONENT_SPACE;
  item.overrides_association = true;
  item.associated_display = mask_bit(1);
  item.associated_space = mask_bit(1);
  bar.sid = 32;
  bar_manager_update_space_components(&manager, false);
  assert(!item.selected);

  puts("mask bounds: passed");
  return 0;
}
