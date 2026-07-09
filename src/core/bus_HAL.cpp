#include "bus_HAL.h"
#include "core/configPins.h"
#include "globals.h"

// Matches the default documented in boards/*/interface.cpp: system peripherals sit on Wire1
// whenever sys_i2c pins differ from i2c_bus, and share Wire otherwise. Boards where that isn't
// true must call setSysI2CBus() during setup.
static TwoWire *sysWire = &Wire1;

static TwoWire *userWire = nullptr;
static bool userBusShared = false;
static int8_t activeSda = -1;
static int8_t activeScl = -1;

void setSysI2CBus(TwoWire *wire) { sysWire = wire; }

TwoWire *getSysI2CBus() { return sysWire; }

TwoWire *acquireI2CBus(int8_t sda, int8_t scl) {
    bool sharesSysBus =
        (gpio_num_t)sda == bruceConfigPins.sys_i2c.sda && (gpio_num_t)scl == bruceConfigPins.sys_i2c.scl;

    if (sharesSysBus) {
        userWire = nullptr;
        userBusShared = true;
        return sysWire;
    }

    TwoWire *wire = (sysWire == &Wire) ? &Wire1 : &Wire;
    if (userWire != wire || activeSda != sda || activeScl != scl) { wire->begin(sda, scl); }
    userWire = wire;
    userBusShared = false;
    activeSda = sda;
    activeScl = scl;
    return wire;
}

TwoWire *acquireI2CBus() {
    return acquireI2CBus((int8_t)bruceConfigPins.i2c_bus.sda, (int8_t)bruceConfigPins.i2c_bus.scl);
}

void releaseI2CBus() {
    if (userBusShared) return;
    if (userWire == nullptr) return;
    userWire->end();
    userWire = nullptr;
    activeSda = -1;
    activeScl = -1;
}
