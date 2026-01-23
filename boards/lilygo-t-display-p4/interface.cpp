// #include "TouchDrvGT9895.hpp"
#include "core/powerSave.h"
#include "core/utils.h"
#include <Wire.h>
#include <interface.h>
// TouchDrvGT9895 touch;

struct TouchPointPro {
    int16_t x = 0;
    int16_t y = 0;
};

#define PIN_SD_CMD 44
#define PIN_SD_CLK 42
#define PIN_SD_D0 39

/***************************************************************************************
** Function name: _setup_gpio()
** Description:   initial setup for the device
***************************************************************************************/
void _setup_gpio() { SD.setPins(PIN_SD_CLK, PIN_SD_CMD, PIN_SD_D0); }

/*********************************************************************
**  Function: setBrightness
**  set brightness value
**********************************************************************/
void _setBrightness(uint8_t brightval) {}

/*********************************************************************
** Function: InputHandler
** Handles the variables PrevPress, NextPress, SelPress, AnyKeyPress and EscPress
**********************************************************************/

void InputHandler(void) { wakeUpScreen(); }

void powerOff() {}

void checkReboot() {}
