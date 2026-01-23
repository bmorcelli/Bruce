#ifndef __IR_MENU_H__
#define __IR_MENU_H__

#include <MenuItemInterface.h>

#if !defined(BRUCE_DISABLE_IR)
class IRMenu : public MenuItemInterface {
public:
    IRMenu() : MenuItemInterface("IR") {}

    void optionsMenu(void);
    void drawIcon(float scale);
    bool hasTheme() { return bruceConfig.theme.ir; }
    String themePath() { return bruceConfig.theme.paths.ir; }

private:
    void configMenu(void);
};
#endif

#endif
