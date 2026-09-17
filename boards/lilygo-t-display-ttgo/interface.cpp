#include "core/powerSave.h"
#include "core/utils.h"
#include <Button.h>

#include <globals.h>
#include <interface.h>

#define ADC_EN 14
#define BTN_ACT LOW
#define DW_BTN 35
#define MINBRIGHT 1
#define UP_BTN 0
volatile bool nxtPress = false;
volatile bool prvPress = false;
volatile bool ecPress = false;
volatile bool slPress = false;
static void onButtonSingleClickCb1(void *button_handle, void *usr_data) { nxtPress = true; }
static void onButtonDoubleClickCb1(void *button_handle, void *usr_data) { slPress = true; }
static void onButtonHoldCb1(void *button_handle, void *usr_data) { slPress = true; }

static void onButtonSingleClickCb2(void *button_handle, void *usr_data) { prvPress = true; }
static void onButtonDoubleClickCb2(void *button_handle, void *usr_data) { ecPress = true; }
static void onButtonHoldCb2(void *button_handle, void *usr_data) { ecPress = true; }

Button *btn1;
Button *btn2;

/***************************************************************************************
** Function name: _setup_gpio()
** Description:   initial setup for the device
***************************************************************************************/
void _setup_gpio() {
    bruceConfigPins.i2c_bus = {(gpio_num_t)21, (gpio_num_t)22}; // sda, scl (Grove)
    bruceConfigPins.rfTx = 21;
    bruceConfigPins.rfRx = 22;
    bruceConfigPins.irTx = 2;
    bruceConfigPins.irRx = 15;
    bruceConfigPins.rotation = 3;
    bruceConfigPins.uart_bus = {(gpio_num_t)13, (gpio_num_t)12};   // rx, tx
    bruceConfigPins.gps_bus = {(gpio_num_t)13, (gpio_num_t)12};    // rx, tx
    bruceConfigPins.badusb_bus = {(gpio_num_t)13, (gpio_num_t)12}; // rx, tx
    bruceConfigPins.SDCARD_bus = {(gpio_num_t)25, (gpio_num_t)27, (gpio_num_t)26, (gpio_num_t)33
    }; // sck,miso,mosi,cs
    // Board's default/generic SPI bus (used by drivers without their own bus, e.g. RC522-SPI)
    bruceConfigPins.outer_bus = {(gpio_num_t)25, (gpio_num_t)27, (gpio_num_t)26, (gpio_num_t)33};
    bruceConfigPins.PN532_bus = {(gpio_num_t)25, (gpio_num_t)27, (gpio_num_t)26, (gpio_num_t)33};
    // CC1101/NRF24/W5500 share the main SPI bus (sck=25, miso=27, mosi=26)
    bruceConfigPins.CC1101_bus = {
        (gpio_num_t)25, (gpio_num_t)27, (gpio_num_t)26, (gpio_num_t)32, (gpio_num_t)39, GPIO_NUM_NC
    }; // sck,miso,mosi,cs,gdo0,gdo2
    bruceConfigPins.NRF24_bus = {
        (gpio_num_t)25, (gpio_num_t)27, (gpio_num_t)26, (gpio_num_t)38, (gpio_num_t)37
    }; // sck,miso,mosi,cs(ss),ce
#if !defined(LITE_VERSION)
    bruceConfigPins.W5500_bus = {
        (gpio_num_t)25, (gpio_num_t)27, (gpio_num_t)26, (gpio_num_t)38, (gpio_num_t)37, GPIO_NUM_NC
    }; // sck,miso,mosi,cs,int,rst
#endif

    // setup buttons
    pinMode(DW_BTN, INPUT_PULLUP);
    pinMode(UP_BTN, INPUT_PULLUP);
    button_config_t bt1 = {
        .type = BUTTON_TYPE_GPIO,
        .long_press_time = 600,
        .short_press_time = 120,
        .gpio_button_config = {
                               .gpio_num = DW_BTN,
                               .active_level = 0,
                               },
    };
    button_config_t bt2 = {
        .type = BUTTON_TYPE_GPIO,
        .long_press_time = 600,
        .short_press_time = 120,
        .gpio_button_config = {
                               .gpio_num = UP_BTN,
                               .active_level = 0,
                               },
    };

    btn1 = new Button(bt1);
    btn1->attachSingleClickEventCb(&onButtonSingleClickCb1, NULL);
    btn1->attachDoubleClickEventCb(&onButtonDoubleClickCb1, NULL);
    btn1->attachLongPressStartEventCb(&onButtonHoldCb1, NULL);

    btn2 = new Button(bt2);
    btn2->attachSingleClickEventCb(&onButtonSingleClickCb2, NULL);
    btn2->attachDoubleClickEventCb(&onButtonDoubleClickCb2, NULL);
    btn2->attachLongPressStartEventCb(&onButtonHoldCb2, NULL);

    // setup POWER pin required by the vendor
    pinMode(ADC_EN, OUTPUT);
    digitalWrite(ADC_EN, HIGH);

    // Start with default IR, RF and RFID Configs, replace old
    bruceConfigPins.rfModule = CC1101_SPI_MODULE;
    bruceConfigPins.rfidModule = PN532_I2C_MODULE;

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

void powerOff() {
    tft.fillScreen(bruceConfig.bgColor);
    digitalWrite(TFT_BL, LOW);
    tft.writecommand(0x10);
    esp_sleep_enable_ext0_wakeup((gpio_num_t)DW_BTN, BTN_ACT);
    esp_deep_sleep_start();
}
