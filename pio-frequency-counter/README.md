# Frequency Counter
Basic example of the species: reads for 25,000 pulses and then prints the time
spent in high, low. Clocks PIO at 100 MHz (divider 1.25) and takes 5 cycles to
read - seems to work OK for 100ns pulses:

![Oscilloscope readout](./scope_100ns.png)

Though not sure how reliable will be below this.