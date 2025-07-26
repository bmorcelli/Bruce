#pragma once
#include "display.h"
#include <globals.h>

String keyboard(String mytext, int maxSize = 76, String msg = "Type your message:");

void __attribute__((weak)) powerOff();
void __attribute__((weak)) goToDeepSleep();

void __attribute__((weak)) checkReboot();

// Shortcut logic

keyStroke _getKeyPress(); // This function must be implemented in the interface.h of the device, in order to
                          // return the key pressed to use as shortcut or input in keyboard environment
                          // by using the flag HAS_KEYBOARD

String
numSelector(int nvalues, int32_t max, int32_t min, String msg, const char c, String initial, int step = 1);
int inline numSelector(String msg, int32_t max, int32_t min, int init, int step = 1) {
    return numSelector(1, max, min, msg, '\0', String(init), step).toInt();
}
// Core functions, depends on the implementation of the funtions above in the interface.h
void checkShortcutPress();
int checkNumberShortcutPress();
char checkLetterShortcutPress();
