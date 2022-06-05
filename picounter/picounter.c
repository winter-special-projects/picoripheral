#include <stdio.h>

#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/i2c.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

#include "clock.pio.h"
#include "counter.pio.h"

volatile uint32_t counter, counts;
volatile bool armed;

// 50000 uint32_t is 200kB i.e. _most_ of the available RAM
#define SIZE 50000
uint32_t data[SIZE];

// i2c setup
#define I2C_ADDR 0x40

#define GPIO_SDA0 4
#define GPIO_SCK0 5

// i2c registers - counts, delay, high, low
volatile uint32_t i2c_params[4];
volatile uint8_t *i2c_registers = (uint8_t *)i2c_params;
volatile uint32_t i2c_offset;

// pio pointers
uint32_t pio_off0, pio_off1;

// pin definitions - should #define
const uint32_t output_pin = 16;
const uint32_t input_pin = 17;
const uint32_t status_pin = 14;

void arm() {
  // set arming on status pin
  gpio_put(status_pin, true);

  // pio0,0 - counter
  pio_off0 = pio_add_program(pio0, &counter_program);
  counter_program_init(pio0, 0, pio_off0, input_pin);

  // pio0,1 - test clock
  pio_off1 = pio_add_program(pio0, &clock_program);
  clock_program_init(pio0, 1, pio_off1, output_pin);

  // clock low into pio; move to isr; push high to pio
  pio0->txf[1] = (i2c_params[3] / 10) - 3;
  pio_sm_exec(pio0, 1, pio_encode_pull(false, false));
  pio_sm_exec(pio0, 1, pio_encode_out(pio_isr, 32));
  pio0->txf[1] = (i2c_params[2] / 10) - 3;

  printf("arm with %d / %d\n", i2c_params[2], i2c_params[3]);

  pio_enable_sm_mask_in_sync(pio0, 0b11);
  armed = true;
}

void disarm() {
  // disarm flag
  armed = false;


  // disable
  pio_sm_set_enabled(pio0, 0, false);
  pio_sm_set_enabled(pio0, 1, false);

  // remove programs to reset PIO
  pio_remove_program(pio0, &counter_program, pio_off0);
  pio_remove_program(pio0, &clock_program, pio_off1);

  // disarm status
  gpio_put(status_pin, false);
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

  // i2c
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

  // sensible defaults...
  i2c_params[1] = 0;
  i2c_params[2] = 50000;
  i2c_params[3] = 50000;

  printf("i2c enabled\n");

  // spi - at demand of 10 MHz
  spi_inst_t *spi = spi1;
  uint32_t baud = spi_init(spi, 10000000);
  spi_set_format(spi, 8, 1, 1, SPI_MSB_FIRST);
  gpio_set_function(10, GPIO_FUNC_SPI);
  gpio_set_function(11, GPIO_FUNC_SPI);
  gpio_set_function(12, GPIO_FUNC_SPI);
  gpio_set_function(13, GPIO_FUNC_SPI);
  spi_set_slave(spi, true);
  printf("spi enabled at %d\n", baud);

  // status pin
  gpio_init(status_pin);
  gpio_set_dir(status_pin, GPIO_OUT);
  gpio_put(status_pin, false);

  // dma
  const uint32_t dma_rx = dma_claim_unused_channel(true);
  dma_channel_config dma_c = dma_channel_get_default_config(dma_rx);
  channel_config_set_transfer_data_size(&dma_c, DMA_SIZE_32);
  channel_config_set_dreq(&dma_c, pio_get_dreq(pio0, 0, false));
  channel_config_set_read_increment(&dma_c, false);
  channel_config_set_write_increment(&dma_c, true);
  printf("dma configured\n");

  armed = false;

  while (true) {
    // wait until we are ready to go
    while (!armed) {
      tight_loop_contents();
    }

    // deploy dma
    dma_channel_configure(dma_rx, &dma_c, (volatile void *) data, (const volatile void *) &(pio0->rxf[0]), i2c_params[0],
                          false);

    // start dma
    dma_channel_start(dma_rx);
    printf("dma started\n");

    // wait for complete
    dma_channel_wait_for_finish_blocking(dma_rx);
    printf("dma completed\n");

    /*
    for (int j = 0; j < i2c_params[0]; j++) {
      data[j] = pio_sm_get_blocking(pio0, 0);
    }
    */

    disarm();

    // fix up data - retain MSB as high / low

    for (int j = 0; j < i2c_params[0]; j++) {
      uint32_t ticks = data[j] - 1;
      if (ticks & 0x80000000) {
        ticks = 50 * (0xffffffff - ticks);
        data[j] = ticks + 0x80000000;
      } else {
        ticks = 50 * (0x7fffffff - ticks);
        data[j] = ticks;
      }
    }
    // spi transfer
    uint8_t *buffer = (uint8_t *)data;
    int transmit =
        spi_write_read_blocking(spi, buffer, buffer, 4 * i2c_params[0]);
    printf("sent %d bytes\n", transmit);
  }
}
