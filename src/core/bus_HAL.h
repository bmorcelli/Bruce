#pragma once

/*
 * HAL for the user-facing I2C bus (bruceConfigPins.i2c_bus).
 *
 * bruceConfigPins.sys_i2c is the bus used by system peripherals (touchscreen, PMIC, RTC, IMU,
 * ...) set up once in boards/*\/interface.cpp and must never be stopped.
 * bruceConfigPins.i2c_bus is user-configurable at runtime and may end up sharing the same
 * physical pins as sys_i2c. This HAL picks the right TwoWire instance for i2c_bus and keeps it
 * from ever colliding with, or shutting down, the sys_i2c bus.
 */

#include <Wire.h>

// Tells the HAL which TwoWire instance is physically wired to bruceConfigPins.sys_i2c.
// Call from boards/*/interface.cpp (_setup_gpio/_post_setup_gpio) only if that board does not use
// the default (&Wire1).
void setSysI2CBus(TwoWire *wire);

// Returns the TwoWire instance currently wired to sys_i2c.
TwoWire *getSysI2CBus();

// Acquires the TwoWire instance for bruceConfigPins.i2c_bus, starting it on the requested pins
// (defaults to bruceConfigPins.i2c_bus.sda/scl). If those pins match sys_i2c, the shared sys bus
// is returned instead of starting a second, colliding bus. Must be paired with releaseI2CBus()
// once the caller is done using the bus.
TwoWire *acquireI2CBus(int8_t sda, int8_t scl);
TwoWire *acquireI2CBus();

// Ends the i2c_bus TwoWire instance started by acquireI2CBus(), unless it turned out to be the
// same physical bus as sys_i2c (which must stay on).
void releaseI2CBus();
