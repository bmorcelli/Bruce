#ifndef __UTILS_H__
#define __UTILS_H__
#include <Arduino.h>
void backToMenu();
void addOptionToMainMenu();
bool updateClockTimezone(bool tmz = false, bool print = false);
void updateTimeStr(struct tm timeInfo);
void showDeviceInfo();
String getOptionsJSON();
void touchHeatMap(struct TouchPoint t);

#endif
