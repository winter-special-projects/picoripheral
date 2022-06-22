import smbus
import struct
import time
import sys

import numpy
from scipy.optimize import curve_fit

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
        spi.max_speed_hz = 10_000_000

        zero = [0 for j in range(2 * self._scan[3])]
        data = bytearray(spi.xfer3(zero))
        spi.close()

        return struct.unpack(f"{self._scan[3]}H", data)

    def time(self):
        delay, high, low, points = self._scan
        return [high + delay + j * (high + low) for j in range(points)]

    def __del__(self):
        GPIO.cleanup()


def f1(t, a, b, c):
    return a * (1 - numpy.exp(-b * t)) + c


def f2(t, a, b, c):
    return a * numpy.exp(-b * t) + c


def main(rc):
    """Determine RC (i.e. product of resistance and capacitance) of serial 
    RC circuit from pump / probe"""

    T = int(round(rc * 1e6, 1))
    dT = max(20, int(T / 20000))

    pico = Picoscope()
    pico.probe(0, dT, dT, 120_000)
    pico.drive(0, T, T, 0)
    pico.arm()
    pico.trigger()
    pico.wait()

    c = int(120_000 * 2 * dT / T)
    n = int(120_000 / c)

    t = numpy.array(pico.time(), dtype=numpy.float64) * 1e-6
    v = numpy.array(pico.read(), dtype=numpy.float64) / 4095.0

    b = []

    for j in range(c):
        _t = t[j * n : (j + 1) * n]
        _t -= _t[0]
        _v = v[j * n : (j + 1) * n]

        if j % 2 == 0:
            p, c = curve_fit(f1, _t, _v)
            b.append(1.0 / p[1])
        else:
            p, c = curve_fit(f2, _t, _v)
            b.append(1.0 / p[1])

    print(f"{numpy.mean(b)} +/- {numpy.std(b)}")


if __name__ == "__main__":
    main(float(sys.argv[1]))
