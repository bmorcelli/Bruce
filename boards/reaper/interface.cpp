#include "core/bus_HAL.h"
#include "core/powerSave.h"
#include <bq27220.h>
#include <globals.h>
#include <interface.h>

#include <Wire.h>
// Power handler for battery detection
#include "core/i2c_finder.h"
#include <Wire.h>
#include <XPowersLib.h>

#define SEL_BTN 0

#define BTN_ACT LOW
#define DW_BTN 40
#define ESC_BTN 21
#define L_BTN 39
#define MINBRIGHT 1
#define R_BTN 38
#define UP_BTN 41
// Charger chip

XPowersPPM PPM;
/***************************************************************************************
** Function name: _setup_gpio()
** Location: main.cpp
** Description:   initial setup for the device
***************************************************************************************/

// BATTERY GAUGE
#define BATTERY_DESIGN_CAPACITY 1000
#include <bq27220.h>
BQ27220 bq;
bool gaugeOn = false;

void _setup_gpio() {
    bruceConfigPins.sys_i2c = {(gpio_num_t)47, (gpio_num_t)48}; // sda, scl
    bruceConfigPins.i2c_bus = {(gpio_num_t)47, (gpio_num_t)48}; // sda, scl (Grove)
    bruceConfigPins.rfTx = 47;
    bruceConfigPins.rfRx = 48;
    bruceConfigPins.irTx = 47;
    bruceConfigPins.irRx = 1;
    bruceConfigPins.rotation = 1;
    bruceConfigPins.uart_bus = {(gpio_num_t)44, (gpio_num_t)43};    // rx, tx
    bruceConfigPins.gps_bus = {(gpio_num_t)44, (gpio_num_t)43};     // rx, tx
    bruceConfigPins.badusb_bus = {(gpio_num_t)48, (gpio_num_t)47};  // rx, tx (CH9329)
    // Board's default/generic SPI bus (used by drivers without their own bus, e.g. RC522-SPI)
    bruceConfigPins.outer_bus = {(gpio_num_t)17, (gpio_num_t)8, (gpio_num_t)18, (gpio_num_t)11};
    bruceConfigPins.PN532_bus = {(gpio_num_t)17, (gpio_num_t)8, (gpio_num_t)18, (gpio_num_t)11};
    // CC1101/NRF24/SDCARD/LoRa/ST25R share the main SPI bus (sck=17, miso=8, mosi=18)
    bruceConfigPins.CC1101_bus = {
        (gpio_num_t)17, (gpio_num_t)8, (gpio_num_t)18, (gpio_num_t)9, (gpio_num_t)46, GPIO_NUM_NC
    }; // sck,miso,mosi,cs,gdo0,gdo2
    bruceConfigPins.NRF24_bus = {
        (gpio_num_t)17, (gpio_num_t)8, (gpio_num_t)18, (gpio_num_t)13, (gpio_num_t)14
    }; // sck,miso,mosi,cs(ss),ce
    bruceConfigPins.SDCARD_bus = {(gpio_num_t)17, (gpio_num_t)8, (gpio_num_t)18, (gpio_num_t)3
    }; // sck,miso,mosi,cs
    bruceConfigPins.ST25R_bus = {
        (gpio_num_t)17, (gpio_num_t)8, (gpio_num_t)18, (gpio_num_t)11, (gpio_num_t)12, GPIO_NUM_NC
    }; // sck,miso,mosi,cs,irq,-
#if !defined(LITE_VERSION)
    bruceConfigPins.LoRa_bus = {
        (gpio_num_t)17, (gpio_num_t)8, (gpio_num_t)18, (gpio_num_t)4, (gpio_num_t)43, GPIO_NUM_NC
    }; // sck,miso,mosi,cs,rst,dio0
#endif

    setSysI2CBus(&Wire); // PMU/battery gauge live on the default Wire object
    Wire.setPins(bruceConfigPins.sys_i2c.sda, bruceConfigPins.sys_i2c.scl);
    Wire.begin(bruceConfigPins.sys_i2c.sda, bruceConfigPins.sys_i2c.scl);

    pinMode(UP_BTN, INPUT); // Sets the power btn as an INPUT
    pinMode(SEL_BTN, INPUT);
    pinMode(DW_BTN, INPUT);
    pinMode(R_BTN, INPUT);
    pinMode(L_BTN, INPUT);
    pinMode(ESC_BTN, INPUT);

    pinMode(bruceConfigPins.CC1101_bus.cs, OUTPUT);
    pinMode(bruceConfigPins.NRF24_bus.cs, OUTPUT);
    pinMode(bruceConfigPins.ST25R_bus.cs, OUTPUT); /// NFC PIN
    digitalWrite(bruceConfigPins.CC1101_bus.cs, HIGH);
    digitalWrite(bruceConfigPins.NRF24_bus.cs, HIGH);
    digitalWrite(bruceConfigPins.ST25R_bus.cs, HIGH);

    // Starts SPI instance for CC1101 and NRF24 with CS pins blocking communication at start

    pinMode(TFT_CS, OUTPUT);
    digitalWrite(TFT_CS, HIGH);
    pinMode(bruceConfigPins.SDCARD_bus.cs, OUTPUT);
    digitalWrite(bruceConfigPins.SDCARD_bus.cs, HIGH);

    bruceConfigPins.rfModule = CC1101_SPI_MODULE;
    bruceConfigPins.rfidModule = ST25R3916_SPI_MODULE;

    bool pmu_ret = false;
    pmu_ret = PPM.init(
        Wire, bruceConfigPins.sys_i2c.sda, bruceConfigPins.sys_i2c.scl, BQ25896_SLAVE_ADDRESS
    );
    if (pmu_ret) {

        PPM.setSysPowerDownVoltage(3300);
        PPM.setInputCurrentLimit(2000);
        Serial.printf("getInputCurrentLimit: %d mA\n", PPM.getInputCurrentLimit());
        PPM.disableCurrentLimitPin();
        PPM.setChargeTargetVoltage(4208);
        PPM.setPrechargeCurr(64);
        PPM.setChargerConstantCurr(832);
        PPM.getChargerConstantCurr();
        Serial.printf("getChargerConstantCurr: %d mA\n", PPM.getChargerConstantCurr());
        PPM.enableMeasure(PowersBQ25896::CONTINUOUS);

        PPM.disableOTG();
        // PPM.enableInputDetection();
        PPM.enableCharge();
    }
    Wire.beginTransmission(BQ27220_I2C_ADDRESS);
    if (Wire.endTransmission() == 0) {
        if (bq.getDesignCap() != BATTERY_DESIGN_CAPACITY) { bq.setDesignCap(BATTERY_DESIGN_CAPACITY); }
        gaugeOn = true;
    }
}

/***************************************************************************************
** Function name: getBattery()
** location: display.cpp

** Description:   Delivers the battery value from 1-100+
***************************************************************************************/
int getBattery() {
    int percent = 0;
#if defined(USE_BQ27220_VIA_I2C)
    if (gaugeOn) percent = bq.getChargePcnt();
#endif

    return (percent < 0) ? 0 : (percent >= 100) ? 100 : percent;
}

#ifdef USE_BQ27220_VIA_I2C
bool isCharging() {
    return gaugeOn ? bq.getIsCharging() : false; // Return the charging status from BQ27220
}
#else
bool isCharging() { return false; }
#endif

/*********************************************************************
** Function: setBrightness
** location: settings.cpp
** set brightness value
**********************************************************************/

void _setBrightness(uint8_t brightval) {
    if (brightval == 0) {
        analogWrite(TFT_BL, brightval);
    } else {
        int bl = MINBRIGHT + round(((255 - MINBRIGHT) * brightval / 100));
        analogWrite(TFT_BL, bl);
    }
}

/*********************************************************************
** Function: InputHandler
** Handles the variables PrevPress, NextPress, SelPress, AnyKeyPress and EscPress
**********************************************************************/
void InputHandler(void) {
    static unsigned long tm = 0;
    if (millis() - tm < 200 && !LongPress) return;
    bool _u = digitalRead(UP_BTN);
    bool _d = digitalRead(DW_BTN);
    bool _l = digitalRead(L_BTN);
    bool _r = digitalRead(R_BTN);
    bool _s = digitalRead(SEL_BTN);
    bool _e = digitalRead(ESC_BTN);

    if (!_s || !_u || !_d || !_r || !_l || !_e) {
        tm = millis();
        if (!wakeUpScreen()) AnyKeyPress = true;
        else return;
    }
    if (!_l) { PrevPress = true; }
    if (!_r) { NextPress = true; }
    if (!_u) {
        UpPress = true;
        PrevPagePress = true;
    }
    if (!_d) {
        DownPress = true;
        NextPagePress = true;
    }
    if (!_s) { SelPress = true; }

    if (!_e) {
        EscPress = true;
        // NextPress = false;
        // PrevPress = false;
    }
}

/*********************************************************************
** Function: powerOff
** location: mykeyboard.cpp
** Turns off the device (or try to)
**********************************************************************/
void powerOff() { PPM.shutdown(); }

/*********************************************************************
** Function: checkReboot
** location: mykeyboard.cpp
** Btn logic to tornoff the device (name is odd btw)
**********************************************************************/
void checkReboot() {
    int countDown;
    /* Long press power off */
    if (digitalRead(ESC_BTN) == BTN_ACT) {
        uint32_t time_count = millis();
        while (digitalRead(ESC_BTN) == BTN_ACT) {
            // Display poweroff bar only if holding button
            if (millis() - time_count > 500) {
                tft.setTextSize(1);
                tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
                countDown = (millis() - time_count) / 1000 + 1;
                if (countDown < 3)
                    tft.drawCentreString("PWR OFF IN " + String(countDown) + "/2", tftWidth / 2, 12, 1);
                else {
                    tft.fillScreen(bruceConfig.bgColor);
                    while (digitalRead(ESC_BTN) == BTN_ACT);
                    delay(200);
                    powerOff();
                }
                delay(10);
            }
        }

        // Clear text after releasing the button
        delay(30);
        tft.fillRect(60, 12, tftWidth - 60, tft.fontHeight(1), bruceConfig.bgColor);
    }
}
