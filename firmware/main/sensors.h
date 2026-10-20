// Sensor reading and fault detection. Two tasks fill the shared sensors_t:
// one for the GPIO/ADC sensors and one for the I2C wattmeter, so a stuck I2C
// bus can't delay the others. A sensor that fails to start is reported as
// failed; it never stops the rest of the firmware.
#pragma once

void sensors_start(void);
