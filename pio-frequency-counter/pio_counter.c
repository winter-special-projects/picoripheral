#include <stdio.h>

#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"

#include "clock.pio.h"
#include "counter.pio.h"

#define SIZE 50000

int main() {
  setup_default_uart();

  uint32_t counts[SIZE];

  const uint32_t output_pin = 16;
  const uint32_t input_pin = 17;

  // pio0 - counter
  // pio1 - test clock

  uint32_t offset0 = pio_add_program(pio0, &counter_program);

  counter_program_init(pio0, 0, offset0, input_pin);

  uint32_t offset1 = pio_add_program(pio1, &clock_program);

  clock_program_init(pio1, 0, offset1, output_pin, 125);

  pio1->txf[0] = 500000 - 3;

  pio_sm_set_enabled(pio1, 0, true);
  pio_sm_set_enabled(pio0, 0, true);

  while (true) {
    for (int j = 0; j < SIZE; j++) {
      counts[j] = pio_sm_get_blocking(pio0, 0);
    }

    for (int j = 0; j < SIZE; j++) {
      uint32_t ticks = counts[j] - 1;
      if (ticks & 0x80000000) {
        ticks = 5 * (0xffffffff - ticks);
        printf("High: %d %d\n", ticks / 10, j);
      } else {
        ticks = 5 * (0x7fffffff - ticks);
        printf("Low:  %d %d\n", ticks / 10, j);
      }
    }
  }
}
