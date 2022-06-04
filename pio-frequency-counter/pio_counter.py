import struct
import time

import smbus
import spidev


class PioCounter:
    def __init__(self):
        # connectivity
        self._i2c_address = 0x40
        self._smbus = smbus.SMBus(1)
        self._count = 0

    def setup(self, count, delay, high, low):
        self._count = count
        message = list(struct.pack("I", count))
        self._smbus.write_i2c_block_data(self._i2c_address, 0x00, message)
        message = list(struct.pack("III", delay, high, low))
        self._smbus.write_i2c_block_data(self._i2c_address, 0x01, message)

    def arm(self):
        self._smbus.write_i2c_block_data(self._i2c_address, 0xFF, [])

    def read(self):
        spi = spidev.SpiDev()
        spi.open(0, 0)
        spi.mode = 3
        spi.bits_per_word = 8
        spi.max_speed_hz = 10_000_000

        zero = [0 for j in range(4 * self._count)]
        data = bytearray(spi.xfer3(zero))
        spi.close()

        return struct.unpack(f"{self._count}I", data)


if __name__ == "__main__":
    pc = PioCounter()
    pc.setup(500, 0, 100, 999_900)
    pc.arm()
    data = pc.read()
    for j, d in enumerate(data):
        print(j, d)
