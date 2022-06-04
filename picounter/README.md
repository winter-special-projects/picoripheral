# Frequency Counter
Basic example of the species: reads for 25,000 pulses and then prints the time
spent in high, low. Clocks PIO at 100 MHz (divider 1.25) and takes 5 cycles to
read - seems to work OK for 100ns pulses:

![Oscilloscope readout](./scope_100ns.png)

Though not sure how reliable will be below this.

## Example
Hook up second pico running MicroPython as a pulse generator, hooking up a PWM to a GPIO pin such as:

```python
from machine import Pin, PWMC
from math import sin

pwm = PWM(Pin(17))
pwm.freq(1000)

for j in range(100):
    for t in range(0, 62832):
        pwm.duty_u16(int(0x8000 + 0x4000 * sin(0.0001 * t)))
```

This will generate PWM pulses on (it would seem) a default frequency of 1 kHz, with duty cycle from 25% to 75%. Hooking up the timer and saving the high and low times from this (sampling from the middle of a "run") gives the following output:

![PWM graph](./pwm.png)
