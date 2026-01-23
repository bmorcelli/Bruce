#ifndef __MAIN_MENU_H__
#define __MAIN_MENU_H__

#include <MenuItemInterface.h>

#if !defined(BRUCE_DISABLE_BLE)
#include "menu_items/BleMenu.h"
#endif
#include "menu_items/ClockMenu.h"
#include "menu_items/ConfigMenu.h"
#if !defined(BRUCE_DISABLE_ESPNOW)
#include "menu_items/ConnectMenu.h"
#endif
#include "menu_items/EthernetMenu.h"
#include "menu_items/FMMenu.h"
#include "menu_items/FileMenu.h"
#include "menu_items/GpsMenu.h"
#if !defined(BRUCE_DISABLE_IR)
#include "menu_items/IRMenu.h"
#endif
#include "menu_items/LoRaMenu.h"
#include "menu_items/NRF24.h"
#include "menu_items/OthersMenu.h"
#include "menu_items/RFIDMenu.h"
#include "menu_items/RFMenu.h"
#include "menu_items/ScriptsMenu.h"
#include "menu_items/WifiMenu.h"
class MainMenu {
public:
    FileMenu fileMenu;
#if !defined(BRUCE_DISABLE_BLE)
    BleMenu bleMenu;
#endif
    ClockMenu clockMenu;
#if !defined(BRUCE_DISABLE_ESPNOW)
    ConnectMenu connectMenu;
#endif
    ConfigMenu configMenu;
    FMMenu fmMenu;
    GpsMenu gpsMenu;
#if !defined(BRUCE_DISABLE_IR)
    IRMenu irMenu;
#endif
    NRF24Menu nrf24Menu;
    OthersMenu othersMenu;
    RFIDMenu rfidMenu;
    RFMenu rfMenu;
    ScriptsMenu scriptsMenu;
    WifiMenu wifiMenu;
#if !defined(LITE_VERSION)
    LoRaMenu loraMenu;
    EthernetMenu ethernetMenu;
#endif

    MainMenu();
    ~MainMenu();

    void begin(void);
    std::vector<MenuItemInterface *> getItems(void) { return _menuItems; }
    void hideAppsMenu();

private:
    int _currentIndex = 0;
    int _totalItems = 0;
    std::vector<MenuItemInterface *> _menuItems;
};
extern MainMenu mainMenu;

#endif
