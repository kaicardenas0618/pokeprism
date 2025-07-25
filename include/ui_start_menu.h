#ifndef GUARD_UI_START_MENU_H
#define GUARD_UI_START_MENU_H

#include "main.h"

struct StartMenuResources
{
    MainCallback savedCallback;
    u8 gfxLoadState;
    u16 cursorSpriteId;
    u16 iconBoxSpriteIds[6];
    u16 iconMonSpriteIds[6];
    u16 iconStatusSpriteIds[6];
    u16 selector_x;
    u16 selector_y;
    u16 selectedMenu;
};

enum StartMenuWindowIds
{
    WINDOW_HP_BARS,
    WINDOW_TOP_BAR,
    WINDOW_BOTTOM_BAR,
};

enum StartMenuBoxes
{
    START_MENU_POKEDEX,
    START_MENU_PARTY,
    START_MENU_BAG,
    START_MENU_CARD,
    START_MENU_MAP,
    START_MENU_OPTIONS,
};

void Task_StartMenu_Open(u8 taskId);
void StartMenu_Init(MainCallback callback);

#endif // GUARD_UI_MENU_H
