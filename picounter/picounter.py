import struct
import time

import smbus
import spidev

from RPi import GPIO


class Picounter:
    def __init__(self):
        # connectivity
        self._i2c_address = 0x40
        self._smbus = smbus.SMBus(1)
        self._count = 0

        # status pin
        GPIO.setmode(GPIO.BCM)
        GPIO.setup(16, GPIO.IN)

    def __del__(self):
        GPIO.cleanup()

    def data(self):
        time.sleep(0.01)
        return GPIO.input(16)

    def setup(self, count, high, low):
        self._count = count
        message = list(struct.pack("I", count))
        self._smbus.write_i2c_block_data(self._i2c_address, 0x00, message)
        message = list(struct.pack("II", high, low))
        self._smbus.write_i2c_block_data(self._i2c_address, 0x01, message)

    def arm(self):
        self._smbus.write_i2c_block_data(self._i2c_address, 0xFF, [])

    def read(self):
        while not pc.data():
            pass

        spi = spidev.SpiDev()
        spi.open(0, 0)
        spi.mode = 3
        spi.bits_per_word = 8
        spi.max_speed_hz = 10_000_000

        zero = [0 for j in range(4 * self._count)]
        print(f"sending {len(zero)} bytes")
        data = bytearray(spi.xfer3(zero))
        spi.close()

        data = struct.unpack(f"{self._count}I", data)
        high = [d - 0x80000000 for d in data if d >= 0x8000000]
        low = [d for d in data if d < 0x8000000]

        return high, low


if __name__ == "__main__":
    pc = Picounter()
    pc.setup(12500, 100, 100)
    pc.arm()
    high, low = pc.read()
    print(len(high), len(low), set(high), set(low))
