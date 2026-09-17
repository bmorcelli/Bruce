#include "core/powerSave.h"
#include "core/utils.h"
#include <Button.h>

#include <globals.h>
#include <interface.h>

#define SEL_BTN 37

#define BTN_ACT LOW
#define DW_BTN 39
#define MINBRIGHT 1
#define UP_BTN 38

volatile bool nxtPress = false;
volatile bool prvPress = false;
volatile bool ecPress = false;
volatile bool slPress = false;

// LEFT (UP_BTN) -> Previous
static void onPrevSingleClickCb(void *button_handle, void *usr_data) { prvPress = true; }
// RIGHT (DW_BTN) -> Next
static void onNextSingleClickCb(void *button_handle, void *usr_data) { nxtPress = true; }
// CENTER (SEL_BTN) -> Select on click, Escape on double-click or hold
static void onSelSingleClickCb(void *button_handle, void *usr_data) { slPress = true; }
static void onSelDoubleClickCb(void *button_handle, void *usr_data) { ecPress = true; }
static void onSelHoldCb(void *button_handle, void *usr_data) { ecPress = true; }

Button *btnPrev;
Button *btnNext;
Button *btnSel;

/***************************************************************************************
** Function name: _setup_gpio()
** Description:   initial setup for the device
***************************************************************************************/
void _setup_gpio() {
    bruceConfigPins.i2c_bus = {(gpio_num_t)21, (gpio_num_t)22}; // sda, scl (Grove)
    bruceConfigPins.rfTx = 21;
    bruceConfigPins.rfRx = 22;
    bruceConfigPins.irTx = 26; // TXLED
    bruceConfigPins.irRx = 25; // RXLED
    bruceConfigPins.rotation = 1;
    bruceConfigPins.uart_bus = {(gpio_num_t)22, (gpio_num_t)21};    // rx, tx
    bruceConfigPins.gps_bus = {(gpio_num_t)22, (gpio_num_t)21};     // rx, tx
    bruceConfigPins.badusb_bus = {(gpio_num_t)22, (gpio_num_t)21};  // rx, tx (CH9329)
    // Board's default/generic SPI bus (used by drivers without their own bus, e.g. RC522-SPI).
    // No dedicated PN532 module on this board, so PN532_bus shares the same slot.
    bruceConfigPins.outer_bus = {(gpio_num_t)25, (gpio_num_t)33, (gpio_num_t)26, (gpio_num_t)17};
    bruceConfigPins.PN532_bus = {(gpio_num_t)25, (gpio_num_t)33, (gpio_num_t)26, (gpio_num_t)17};
    // No RF module fitted on the T4; these are sane defaults on free GPIOs so CC1101/NRF24
    // modules can be wired manually (ALLOW_ALL_GPIO_FOR_IR_RF lets the user re-map at runtime).
    bruceConfigPins.CC1101_bus = {
        (gpio_num_t)25, (gpio_num_t)33, (gpio_num_t)26, (gpio_num_t)17, (gpio_num_t)34, GPIO_NUM_NC
    }; // sck,miso,mosi,cs,gdo0,gdo2
    bruceConfigPins.NRF24_bus = {
        (gpio_num_t)25, (gpio_num_t)33, (gpio_num_t)26, (gpio_num_t)17, (gpio_num_t)35
    }; // sck,miso,mosi,cs(ss),ce
    bruceConfigPins.SDCARD_bus = {(gpio_num_t)14, (gpio_num_t)2, (gpio_num_t)15, (gpio_num_t)13
    }; // sck,miso,mosi,cs

    // The three front buttons are on ESP32 input-only GPIOs (37/38/39), which
    // cannot use the internal pull-ups - the T4 board provides external ones,
    // so they are configured as plain INPUT and read as active LOW.
    pinMode(UP_BTN, INPUT);
    pinMode(DW_BTN, INPUT);
    pinMode(SEL_BTN, INPUT);

    button_config_t btPrev = {
        .type = BUTTON_TYPE_GPIO,
        .long_press_time = 600,
        .short_press_time = 120,
        .gpio_button_config =
            {
                       .gpio_num = UP_BTN,
                       .active_level = 0,
                       },
    };
    button_config_t btNext = {
        .type = BUTTON_TYPE_GPIO,
        .long_press_time = 600,
        .short_press_time = 120,
        .gpio_button_config =
            {
                       .gpio_num = DW_BTN,
                       .active_level = 0,
                       },
    };
    button_config_t btSel = {
        .type = BUTTON_TYPE_GPIO,
        .long_press_time = 600,
        .short_press_time = 120,
        .gpio_button_config =
            {
                       .gpio_num = SEL_BTN,
                       .active_level = 0,
                       },
    };

    btnPrev = new Button(btPrev);
    btnPrev->attachSingleClickEventCb(&onPrevSingleClickCb, NULL);

    btnNext = new Button(btNext);
    btnNext->attachSingleClickEventCb(&onNextSingleClickCb, NULL);

    btnSel = new Button(btSel);
    btnSel->attachSingleClickEventCb(&onSelSingleClickCb, NULL);
    btnSel->attachDoubleClickEventCb(&onSelDoubleClickCb, NULL);
    btnSel->attachLongPressStartEventCb(&onSelHoldCb, NULL);

    Serial.begin(115200);
}

/*********************************************************************
**  Function: setBrightness
**  set brightness value
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
    static bool btn_pressed = false;
    if (nxtPress || prvPress || ecPress || slPress) btn_pressed = true;

    if (millis() - tm > 200 || LongPress) {
        if (btn_pressed) {
            btn_pressed = false;
            tm = millis();
            if (!wakeUpScreen()) AnyKeyPress = true;
            else return;
            SelPress = slPress;
            EscPress = ecPress;
            NextPress = nxtPress;
            PrevPress = prvPress;

            nxtPress = false;
            prvPress = false;
            ecPress = false;
            slPress = false;
        }
    }
}

/*********************************************************************
** Function: powerOff
** Turns off the device (deep sleep, wake on the center button)
**********************************************************************/
void powerOff() {
    tft.fillScreen(bruceConfig.bgColor);
    digitalWrite(TFT_BL, LOW);
    tft.writecommand(0x10); // ILI9341 enter sleep
    esp_sleep_enable_ext0_wakeup((gpio_num_t)SEL_BTN, BTN_ACT);
    esp_deep_sleep_start();
}
