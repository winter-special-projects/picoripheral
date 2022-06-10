import smbus
import struct
import time

from RPi import GPIO
import spidev


class Picoscope:
    def __init__(self):
        # connectivity
        self._i2c_address = 0x42
        self._smbus = smbus.SMBus(1)

        # trigger GPIO
        GPIO.setmode(GPIO.BCM)
        GPIO.setup(16, GPIO.OUT)

        # scan data
        self._scan = (0, 0, 0, 0)

    def probe(self, delay, high, low, points):
        self._scan = (delay, high, low, points)
        message = list(struct.pack("IIII", delay, high, low, points))
        self._smbus.write_i2c_block_data(self._i2c_address, 0x10, message)

    def drive(self, delay, high, low, points):
        message = list(struct.pack("IIII", delay, high, low, points))
        self._smbus.write_i2c_block_data(self._i2c_address, 0x11, message)

    def arm(self):
        self._smbus.write_i2c_block_data(self._i2c_address, 0xFF, [])

    def trigger(self):
        GPIO.output(16, GPIO.HIGH)
        time.sleep(0.01)
        GPIO.output(16, GPIO.LOW)

    def wait(self):
        delay, high, low, points = self._scan
        time.sleep(1e-6 * (delay + points * (high + low)) + 0.1)

    def read(self):
        spi = spidev.SpiDev()
        spi.open(0, 0)
        spi.mode = 3
        spi.bits_per_word = 8
        spi.max_speed_hz = 10000000

        zero = [0 for j in range(2 * self._scan[3])]
        data = bytearray(spi.xfer3(zero))
        spi.close()

        return struct.unpack(f"{self._scan[3]}H", data)

    def time(self):
        delay, high, low, points = self._scan
        return [high + delay + j * (high + low) for j in range(points)]

    def __del__(self):
        GPIO.cleanup()


if __name__ == "__main__":
    pico = Picoscope()
    pico.probe(0, 50, 50, 6000)
    pico.drive(0, 100, 100, 0)
    pico.arm()
    pico.trigger()
    pico.wait()
    data = pico.read()
    time = pico.time()

    for t, d in zip(time, data):
        print(t, d)
