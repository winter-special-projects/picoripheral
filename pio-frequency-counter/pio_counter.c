#include <stdio.h>

#include "hardware/clocks.h"
#include "hardware/i2c.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"

#include "clock.pio.h"
#include "counter.pio.h"

// 50000 uint32_t is 200kB i.e. _most_ of the available RAM
#define SIZE 50000

volatile uint32_t counter, counts;
volatile bool armed;
uint32_t data[SIZE];

// i2c setup
#define I2C_ADDR 0x40

#define GPIO_SDA0 4
#define GPIO_SCK0 5

// i2c registers - counts, delay, high, low
uint32_t i2c_params[4];
uint8_t *i2c_registers = (uint8_t *)i2c_params;
uint32_t i2c_offset;

void arm() {
  // clock low into pio; move to isr; push high to pio
  pio1->txf[0] = (i2c_params[3] / 10) - 3;
  pio_sm_exec(pio1, 0, pio_encode_pull(false, false));
  pio_sm_exec(pio1, 0, pio_encode_out(pio_isr, 32));
  pio1->txf[0] = (i2c_params[2] / 10) - 3;

  pio_sm_set_enabled(pio1, 0, true);
  pio_sm_set_enabled(pio0, 0, true);
  armed = true;
}

void disarm() {
  pio_sm_set_enabled(pio1, 0, false);
  pio_sm_set_enabled(pio0, 0, false);
}

void i2c0_handler() {
  uint32_t status = i2c0_hw->intr_stat;
  uint32_t value;

  uint8_t command;

  if (status & I2C_IC_INTR_STAT_R_RX_FULL_BITS) {
    value = i2c0_hw->data_cmd;

    if (value & I2C_IC_DATA_CMD_FIRST_DATA_BYTE_BITS) {
      command = (uint8_t)(value & I2C_IC_DATA_CMD_BITS);
      if (command == 0xff) {
        arm();
      } else if (command == 0x00) {
        // set counts
        i2c_offset = 0x0;
      } else if (command == 0x01) {
        // write clock settings
        i2c_offset = 0x4;
      }
    } else {
      // copy in settings
      i2c_registers[i2c_offset++] = (uint8_t)(value & I2C_IC_DATA_CMD_BITS);
    }
  }
}

int main() {
  setup_default_uart();

  // set up i2c
  i2c_init(i2c0, 100e3);
  i2c_set_slave_mode(i2c0, true, I2C_ADDR);

  gpio_set_function(GPIO_SDA0, GPIO_FUNC_I2C);
  gpio_set_function(GPIO_SCK0, GPIO_FUNC_I2C);
  gpio_pull_up(GPIO_SDA0);
  gpio_pull_up(GPIO_SCK0);

  i2c0_hw->intr_mask =
      I2C_IC_INTR_MASK_M_RD_REQ_BITS | I2C_IC_INTR_MASK_M_RX_FULL_BITS;

  irq_set_exclusive_handler(I2C0_IRQ, i2c0_handler);
  irq_set_enabled(I2C0_IRQ, true);

  const uint32_t output_pin = 16;
  const uint32_t input_pin = 17;

  // pio0 - counter
  // pio1 - test clock

  uint32_t offset0 = pio_add_program(pio0, &counter_program);

  counter_program_init(pio0, 0, offset0, input_pin);

  uint32_t offset1 = pio_add_program(pio1, &clock_program);

  clock_program_init(pio1, 0, offset1, output_pin);

  armed = false;

  // sensible defaults...
  i2c_params[1] = 0;
  i2c_params[2] = 50000;
  i2c_params[3] = 50000;

  while (true) {
    // wait until we are ready to go
    while (!armed) {
      tight_loop_contents();
    }

    for (int j = 0; j < i2c_params[0]; j++) {
      data[j] = pio_sm_get_blocking(pio0, 0);
    }

    disarm();

    for (int j = 0; j < i2c_params[0]; j++) {
      uint32_t ticks = data[j] - 1;
      if (ticks & 0x80000000) {
        ticks = 5 * (0xffffffff - ticks);
        printf("High: %d %d\n", ticks * 100, j);
      } else {
        ticks = 5 * (0x7fffffff - ticks);
        printf("Low:  %d %d\n", ticks * 100, j);
      }
    }
  }
}
