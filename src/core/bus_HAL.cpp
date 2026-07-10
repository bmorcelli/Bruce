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

// ---------------- SPI bus arbitration ----------------

// Pins currently configured on the shared auxiliary bus (AUX_SPI), so repeated acquisitions
// with the same pins skip a redundant end()/begin() cycle that could disturb whoever else is
// mid-transaction on it.
static gpio_num_t sharedSpiSck = GPIO_NUM_NC;
static gpio_num_t sharedSpiMiso = GPIO_NUM_NC;
static gpio_num_t sharedSpiMosi = GPIO_NUM_NC;

static SPIClass *acquireSharedSPI(gpio_num_t sck, gpio_num_t miso, gpio_num_t mosi) {
    if (sharedSpiSck != sck || sharedSpiMiso != miso || sharedSpiMosi != mosi) {
        if (sharedSpiSck != GPIO_NUM_NC) AUX_SPI.end();
        AUX_SPI.begin((int8_t)sck, (int8_t)miso, (int8_t)mosi);
        sharedSpiSck = sck;
        sharedSpiMiso = miso;
        sharedSpiMosi = mosi;
    }
    return &AUX_SPI;
}

SPIClass *acquireSPIBus(gpio_num_t sck, gpio_num_t miso, gpio_num_t mosi) {
    if (sck == GPIO_NUM_NC || miso == GPIO_NUM_NC || mosi == GPIO_NUM_NC) return nullptr;

#if TFT_MOSI > 0
    // Same physical wiring as the display: it already owns a hardware controller, so reuse it
    // instead of trying to claim a second one on the same pins.
    if (mosi == (gpio_num_t)TFT_MOSI) return &tft.getSPIinstance();
#endif

    // Same physical wiring as the SD card: it is mounted for the whole program lifetime, so its
    // bus is already up and must not be reconfigured.
    if (bruceConfigPins.SDCARD_bus.mosi != GPIO_NUM_NC && mosi == bruceConfigPins.SDCARD_bus.mosi) {
        return &sdcardSPI;
    }

    // Neither the display nor the SD card own these pins: fall back to the one remaining hardware
    // SPI controller, shared across every other peripheral one owner at a time.
    return acquireSharedSPI(sck, miso, mosi);
}
