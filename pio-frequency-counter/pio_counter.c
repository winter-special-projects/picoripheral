#include <stdio.h>

#include "hardware/clocks.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"

#include "clock.pio.h"
#include "counter.pio.h"

// we can store at most arounf 50000 uint32_t
#define SIZE 50000

volatile uint32_t counter, counts;
uint32_t data[SIZE];

void arm();
void disarm();

// i2c setup
#define I2C_ADDR 0x40

#define GPIO_SDA0 4
#define GPIO_SCK0 5

// i2c registers - counts, delay, high, low
uint32_t i2c_params[4];
uint8_t *i2c_registers = (uint8_t *)i2c_params;
uint32_t i2c_offset;

void i2c0_handler() {
  uint32_t status = i2c0_hw->intr_stat;
  uint32_t value;

  if (status & I2C_IC_INTR_STAT_R_RX_FULL_BITS) {
    value = i2c0_hw->data_cmd;

    if (value & I2C_IC_DATA_CMD_FIRST_DATA_BYTE_BITS) {
      command = (uint8_t)(value & I2C_IC_DATA_CMD_BITS);
      if (command == 0xff) {
        arm();
      } else if (command == 0x00) {
        // set counts
        offset = 0x0;
      } else if (command == 0x01) {
        // write driver settings
        offset = 0x4;
      }
    } else {
      // copy in settings
      i2c_registers[offset++] = (uint8_t)(value & I2C_IC_DATA_CMD_BITS);
    }
  }
}

int main() {
  setup_default_uart();

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
