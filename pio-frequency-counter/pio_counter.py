import smbus
import struct
import time


class PioCounter:
    def __init__(self):
        # connectivity
        self._i2c_address = 0x40
        self._smbus = smbus.SMBus(1)

    def setup(self, count, delay, high, low):
        message = list(struct.pack("I", count))
        self._smbus.write_i2c_block_data(self._i2c_address, 0x00, message)
        message = list(struct.pack("III", delay, high, low))
        self._smbus.write_i2c_block_data(self._i2c_address, 0x01, message)

    def arm(self):
        self._smbus.write_i2c_block_data(self._i2c_address, 0xFF, [])


if __name__ == "__main__":
    pc = PioCounter()
    pc.setup(50000, 0, 100, 900)
    pc.arm()
