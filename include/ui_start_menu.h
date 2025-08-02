#ifndef GUARD_UI_START_MENU_H
#define GUARD_UI_START_MENU_H

#include "main.h"

struct StartMenuResources
{
    MainCallback savedCallback;
    u8 gfxLoadState;
    u8 MenuButtonSpriteIds[14];
    u16 cursorSpriteIds[2];
    //u16 iconBoxSpriteIds[6];
    u16 iconMonSpriteIds[6];
    u16 iconStatusSpriteIds[6];
    u16 selector_y;
    u8 scrollOffset;
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
    START_MENU_POKEMON,
    START_MENU_BAG,
    START_MENU_CARD,
    START_MENU_QUESTS,
    START_MENU_OPTIONS,
    START_MENU_DEXNAV,
};

#define START_MENU_BUTTON_X_1       126
#define START_MENU_BUTTON_X_2       START_MENU_BUTTON_X_1 + 64
#define START_MENU_BUTTON_Y_START   32
#define START_MENU_BUTTON_Y_SPACING 32

enum MenuButtons
{
    START_MENU_BUTTON_POKEDEX1,
    START_MENU_BUTTON_POKEDEX2,
    START_MENU_BUTTON_POKEMON1,
    START_MENU_BUTTON_POKEMON2,
    START_MENU_BUTTON_BAG1,
    START_MENU_BUTTON_BAG2,
    START_MENU_BUTTON_CARD1,
    START_MENU_BUTTON_CARD2,
    START_MENU_BUTTON_QUESTS1,
    START_MENU_BUTTON_QUESTS2,
    START_MENU_BUTTON_OPTIONS1,
    START_MENU_BUTTON_OPTIONS2,
    START_MENU_BUTTON_DEXNAV1,
    START_MENU_BUTTON_DEXNAV2,
    START_MENU_BUTTON_COUNT,
};

void Task_StartMenu_Open(u8 taskId);
void StartMenu_Init(MainCallback callback);

#endif // GUARD_UI_MENU_H
