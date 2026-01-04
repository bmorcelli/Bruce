#ifndef __LORA_MENU_H__
#define __LORA_MENU_H__
#if !defined(LITE_VERSION)
#include <MenuItemInterface.h>

void lorachat();
void changeusername();
void chfreq();

class LoRaMenu : public MenuItemInterface {
private:
    void configMenu(void);

public:
    LoRaMenu() : MenuItemInterface("LoRa") {}

    void optionsMenu(void);
    void drawIcon(float scale);
    void drawIconImg();
    bool getTheme() { return bruceConfig.theme.lora; }
};

#endif
#endif
