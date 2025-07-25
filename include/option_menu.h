#ifndef GUARD_OPTION_MENU_H
#define GUARD_OPTION_MENU_H

#define tMenuSelection   data[0]
#define tCurrentPage     data[1]
#define tPageSelection   data[2]
#define tTextSpeed       data[3]
#define tBattleScene     data[4]
#define tBattleStyle     data[5]
#define tSound           data[6]
#define tButtonMode      data[7]
#define tWindowFrameType data[8]
#define tScrollBgs       data[9]
#define tClockMode       data[10]

enum
{
    MENUITEM_TEXTSPEED,
    MENUITEM_BATTLESCENE,
    MENUITEM_BATTLESTYLE,
    MENUITEM_SOUND,
    MENUITEM_BUTTONMODE,
    MENUITEM_FRAMETYPE,
    MENUITEM_SCROLLBGS,
    MENUITEM_CLOCKMODE,
    MENUITEM_COUNT,
};

enum
{
    WIN_HEADER,
    WIN_OPTIONS
};

enum
{
    PAGE_GENERAL,
    PAGE_BATTLE,
    PAGE_UI,
    PAGE_COUNT
};

void CB2_InitOptionMenu(void);

#endif // GUARD_OPTION_MENU_H
