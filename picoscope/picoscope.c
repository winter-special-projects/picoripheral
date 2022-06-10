#include <stdio.h>

#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

#include "delay.pio.h"
#include "timer.pio.h"

// GPIO assignments
#define EXTERNAL 14
#define COUNTER 15
#define CLOCK0 16
#define CLOCK1 17

#define LED 25
#define ADC0 26

#define GPIO_SDA0 4
#define GPIO_SCK0 5

// static definitions
#define I2C_ADDR 0x42

#define REG_READ 0x10
#define REG_DRV 0x11
#define REG_ARM 0xff

// internal timer setup function
void timer(PIO pio, uint sm, uint pin, uint32_t delay, uint32_t high,
           uint32_t low, bool enable);

// sm program offsets and program pointer
uint32_t offsets[2];
const pio_program_t *programs[2];

// arm and disarm functions
void arm();
void disarm();

volatile uint32_t counter, counts;
volatile uint64_t t0, t1;

volatile uint16_t readout;

// data registers - 4 uint32_t for driver then reader
uint32_t driver_reader[8];
uint32_t *driver = driver_reader;
uint32_t *reader = driver_reader + 4;

// stored data array - 120000 points is enough for 120s at 1kHz
#define SIZE 120000
uint16_t data[SIZE];

// i2c helpers
uint8_t *driver_reader_bytes = (uint8_t *)driver_reader;
uint32_t offset, command;

// i2c handler
void i2c0_handler() {
  uint32_t status = i2c0_hw->intr_stat;
  uint32_t value;

  // write request
  if (status & I2C_IC_INTR_STAT_R_RX_FULL_BITS) {
    value = i2c0_hw->data_cmd;

    if (value & I2C_IC_DATA_CMD_FIRST_DATA_BYTE_BITS) {
      command = (uint8_t)(value & I2C_IC_DATA_CMD_BITS);
      if (command == 0xff) {
        arm();
      } else if (command == 0x10) {
        offset = 0x10;
      } else if (command == 0x11) {
        offset = 0x0;
      }
    } else {
      driver_reader_bytes[offset++] = (uint8_t)(value & I2C_IC_DATA_CMD_BITS);
    }
  }
}

// GPIO IRQ callback
void __not_in_flash_func(callback)(uint gpio, uint32_t event) {
  if (gpio == COUNTER) {
    if (event == GPIO_IRQ_EDGE_FALL) {
      data[counter] = adc_read();
      counter++;
      if (counter == counts) {
        pio_sm_set_enabled(pio1, 0, false);
        pio_sm_set_enabled(pio1, 1, false);
        disarm();
        t1 = time_us_64();
        gpio_put(LED, false);
        printf("%d %ld\n", counter, t1 - t0);
      }
    }
  } else if (gpio == EXTERNAL) {
    // trigger counters
    if (event == GPIO_IRQ_EDGE_RISE) {
      counter = 0;
      gpio_put(LED, true);
      t0 = time_us_64();
      pio_enable_sm_mask_in_sync(pio1, 0b11);
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

  // adc
  adc_init();
  adc_gpio_init(ADC0);
  adc_select_input(0);

  // led
  gpio_init(LED);
  gpio_set_dir(LED, GPIO_OUT);
  gpio_put(LED, false);

  // spi - at demand of 10 MHz
  spi_inst_t *spi = spi1;
  uint32_t baud = spi_init(spi, 10000000);
  spi_set_format(spi, 8, 1, 1, SPI_MSB_FIRST);
  gpio_set_function(10, GPIO_FUNC_SPI);
  gpio_set_function(11, GPIO_FUNC_SPI);
  gpio_set_function(12, GPIO_FUNC_SPI);
  gpio_set_function(13, GPIO_FUNC_SPI);
  spi_set_slave(spi, true);
  printf("Initialised SPI at %d\n", baud);

  uint32_t freq = clock_get_hz(clk_sys);

  freq /= 25;
  printf("Functional frequency: %d\n", freq);

  // set up the IRQ
  uint32_t irq_mask = GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL;
  gpio_set_irq_enabled_with_callback(COUNTER, irq_mask, true, &callback);
  gpio_set_irq_enabled(EXTERNAL, irq_mask, true);

  while (true) {
    if (counter != counts || counts == 0)
      continue;
    // transmit content of buffer up SPI link - writing nonsense back into
    // buffer as we go...
    uint8_t *buffer = (uint8_t *)data;
    int transmit = spi_write_read_blocking(spi, buffer, buffer, 2 * counts);
    counter = 0;
    printf("Sent %d bytes\n", transmit);
  }
}

void arm() {
  // debug info
  printf("D: %d %d %d %d\n", driver[0], driver[1], driver[2], driver[3]);
  printf("R: %d %d %d %d\n", reader[0], reader[1], reader[2], reader[3]);

  // driver clock
  timer(pio1, 1, CLOCK1, driver[0], driver[1], driver[2], false);

  // reader clock
  counts = reader[3];
  timer(pio1, 0, CLOCK0, reader[0], reader[1], reader[2], false);

  // TBD do I want to set up the interrupts in here?
}

void disarm() {
  // remove programs from PIO blocks - we have two because two state machines
  pio_remove_program(pio1, programs[0], offsets[0]);
  pio_remove_program(pio1, programs[1], offsets[1]);
  printf("Disarm\n");
}

// with-delay timer program
void timer(PIO pio, uint sm, uint pin, uint32_t delay, uint32_t high,
           uint32_t low, bool enable) {
  // if delay, load one program else other
  // set clock divider to give ~ 0.2µs / tick (i.e. /= 25)
  delay *= 5;
  high *= 5;
  low *= 5;
  if (delay == 0) {
    programs[sm] = &timer_program;
    offsets[sm] = pio_add_program(pio, &timer_program);
    timer_program_init(pio, sm, offsets[sm], pin, 25);
    // intrinsic delays - I _think_ the delay on 1st cycle is 2 not 3
    delay -= 2;
  } else {
    programs[sm] = &delay_program;
    offsets[sm] = pio_add_program(pio, &delay_program);
    delay_program_init(pio, sm, offsets[sm], pin, 25);
    delay -= 2;
  }

  // intrinsic delays - these are certainly 3 cycles
  high -= 3;
  low -= 3;

  if (delay != 0) {
    // load delay into OSR then copy to Y if we are running delay program
    // otherwise Y register unused
    pio->txf[sm] = delay;
    pio_sm_exec(pio, sm, pio_encode_pull(false, false));
    pio_sm_exec(pio, sm, pio_encode_out(pio_y, 32));
  }

  // load low into OSR then copy to ISR
  pio->txf[sm] = low;
  pio_sm_exec(pio, sm, pio_encode_pull(false, false));
  pio_sm_exec(pio, sm, pio_encode_out(pio_isr, 32));

  // load high into OSR
  pio->txf[sm] = high;

  // optionally enable
  pio_sm_set_enabled(pio, sm, enable);
}
