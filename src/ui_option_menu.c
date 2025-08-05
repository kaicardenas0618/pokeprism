#include "global.h"
#include "ui_option_menu.h"
#include "main.h"
#include "menu.h"
#include "scanline_effect.h"
#include "palette.h"
#include "sprite.h"
#include "task.h"
#include "malloc.h"
#include "bg.h"
#include "gpu_regs.h"
#include "window.h"
#include "text.h"
#include "text_window.h"
#include "international_string_util.h"
#include "strings.h"
#include "gba/m4a_internal.h"
#include "constants/rgb.h"
#include "menu_helpers.h"
#include "decompress.h"

enum
{
    MENU_GENERAL,
    MENU_BATTLE,
    MENU_INTERFACE,
    MENU_COUNT,
};

// Menu items
enum
{
    MENUITEM_GENERAL_TEXTSPEED,
    MENUITEM_GENERAL_SOUND,
    MENUITEM_GENERAL_BUTTONMODE,
    MENUITEM_GENERAL_SHINYODDS,
    MENUITEM_GENERAL_COUNT,
};

enum
{
    MENUITEM_BATTLE_BATTLESCENE,
    MENUITEM_BATTLE_BATTLESTYLE,
    MENUITEM_BATTLE_COUNT,
};

enum
{
    MENUITEM_INTERFACE_FRAMETYPE,
    MENUITEM_INTERFACE_SCROLLBGS,
    MENUITEM_INTERFACE_CLOCKMODE,
    MENUITEM_INTERFACE_IVEVDISPLAY,
    MENUITEM_INTERFACE_COUNT,
};

// Window Ids
enum
{
    WIN_TOPBAR,
    WIN_OPTIONS,
    WIN_DESCRIPTION
};

static const struct WindowTemplate sOptionMenuWinTemplates[] =
{
    {//WIN_TOPBAR
        .bg = 1,
        .tilemapLeft = 0,
        .tilemapTop = 0,
        .width = 30,
        .height = 2,
        .paletteNum = 0,
        .baseBlock = 2
    },
    {//WIN_OPTIONS
        .bg = 0,
        .tilemapLeft = 2,
        .tilemapTop = 3,
        .width = 26,
        .height = 10,
        .paletteNum = 0,
        .baseBlock = 62
    },
    {//WIN_DESCRIPTION
        .bg = 1,
        .tilemapLeft = 2,
        .tilemapTop = 15,
        .width = 26,
        .height = 4,
        .paletteNum = 0,
        .baseBlock = 500
    },
    DUMMY_WIN_TEMPLATE
};

static const struct BgTemplate sOptionMenuBgTemplates[] =
{
    {
       .bg = 0,
       .charBaseIndex = 1,
       .mapBaseIndex = 30,
       .screenSize = 0,
       .paletteMode = 0,
       .priority = 1,
    },
    {
       .bg = 1,
       .charBaseIndex = 1,
       .mapBaseIndex = 31,
       .screenSize = 0,
       .paletteMode = 0,
       .priority = 0,
    },
    {
       .bg = 2,
       .charBaseIndex = 0,
       .mapBaseIndex = 29,
       .screenSize = 0,
       .paletteMode = 0,
       .priority = 1,
    },
    {
       .bg = 3,
       .charBaseIndex = 3,
       .mapBaseIndex = 27,
       .screenSize = 0,
       .paletteMode = 0,
       .priority = 2,
    },
};

struct OptionMenu
{
    u8 submenu;
    u8 selGeneral[MENUITEM_GENERAL_COUNT];
    u8 selBattle[MENUITEM_BATTLE_COUNT];
    u8 selInterface[MENUITEM_INTERFACE_COUNT];
    int menuCursor[MENU_COUNT];
    int visibleCursor[MENU_COUNT];
    u8 arrowTaskId;
    u8 gfxLoadState;
};

#define Y_DIFF 16
#define OPTIONS_ON_SCREEN 5
#define NUM_OPTIONS_FROM_BORDER 1

// local functions
static void MainCB2(void);
static void VBlankCB(void);
static void DrawTopBarText(void); //top Option text
static void DrawLeftSideOptionText(int selection, int y);
static void DrawRightSideChoiceText(const u8 *str, int x, int y, bool8 choosen, bool8 active);
static void DrawOptionMenuTexts(void); //left side text;
static void DrawChoices(u32 id, int y); //right side draw function
static void HighlightOptionMenuItem(void);
static void Task_OptionMenuFadeIn(u8 taskId);
static void Task_OptionMenuProcessInput(u8 taskId);
static void Task_OptionMenuSave(u8 taskId);
static void Task_OptionMenuFadeOut(u8 taskId);
static void ScrollMenu(int direction);
static void ScrollAll(int direction); // to bottom or top
static int ProcessInput_TextSpeed(int selection);
static int ProcessInput_Sound(int selection);
static int ProcessInput_ButtonMode(int selection);
static int ProcessInput_ShinyOdds(int selection);
static int ProcessInput_BattleScene(int selection);
static int ProcessInput_BattleStyle(int selection);
static int ProcessInput_FrameType(int selection);
static int ProcessInput_ScrollBgs(int selection);
static int ProcessInput_ClockMode(int selection);
static int ProcessInput_IVEVDisplay(int selection);
static const u8 *const OptionTextDescription(void);
static const u8 *const OptionTextRight(u8 menuItem);
static u8 MenuItemCount(void);
static void DrawDescriptionText(void);
static void DrawOptionMenuChoice(const u8 *text, u8 x, u8 y, u8 style, bool8 active);
static void ReDrawAll(void);
static void DrawChoices_TextSpeed(int selection, int y);
static void DrawChoices_BattleScene(int selection, int y);
static void DrawChoices_BattleStyle(int selection, int y);
static void DrawChoices_Sound(int selection, int y);
static void DrawChoices_ButtonMode(int selection, int y);
static void DrawChoices_ShinyOdds(int selection, int y);
static void DrawChoices_FrameType(int selection, int y);
static void DrawChoices_ScrollBgs(int selection, int y);
static void DrawChoices_ClockMode(int selection, int y);
static void DrawChoices_IVEVDisplay(int selection, int y);
static void DrawBgWindowFrames(void);

// EWRAM vars
EWRAM_DATA static struct OptionMenu *sOptions = NULL;
static EWRAM_DATA u8 *sBg2TilemapBuffer = NULL;
static EWRAM_DATA u8 *sBg3TilemapBuffer = NULL;

// const data
static const u16 sOptionMenuBg_Pal[] = {RGB(17, 18, 31)};
static const u16 sOptionMenuText_Pal[] = INCBIN_U16("graphics/option_menu/ui_text.gbapal");

static const u32 sOptionsMenuTiles[] = INCBIN_U32("graphics/option_menu/tiles.4bpp.lz");
static const u16 sOptionsMenuPalette[] = INCBIN_U16("graphics/option_menu/tiles.gbapal");
static const u32 sOptionsMenuTilemap[] = INCBIN_U32("graphics/option_menu/tiles.bin.lz");

// Scrolling Background
static const u32 sScrollBgTiles[] = INCBIN_U32("graphics/option_menu/scroll_tiles.4bpp.lz");
static const u32 sScrollBgTilemap[] = INCBIN_U32("graphics/option_menu/scroll_tiles.bin.lz");
static const u16 sScrollBgPalette[] = INCBIN_U16("graphics/option_menu/scroll_tiles.gbapal");

// Option Texts
static const u8 *const sTextSpeedOptions[] = {
    gText_TextSpeedSlow,
    gText_TextSpeedMid,
    gText_TextSpeedFast,
};

static const u8 *const sSoundOptions[] = {
    gText_SoundMono,
    gText_SoundStereo,
};

static const u8 *const sButtonModeOptions[] = {
    gText_ButtonTypeNormal,
    gText_ButtonTypeLR,
    gText_ButtonTypeLEqualsA,
};

static const u8 *const sShinyOddsOptions[] = {
    gText_ShinyOdds8192,
    gText_ShinyOdds4096,
    gText_ShinyOdds2048,
    gText_ShinyOdds1024,
    gText_ShinyOdds512,
};

static const u8 *const sBattleSceneOptions[] = {
    gText_BattleSceneOn,
    gText_BattleSceneOff,
};

static const u8 *const sBattleStyleOptions[] = {
    gText_BattleStyleShift,
    gText_BattleStyleSet,
};

static const u8 *const sFrameTypeOptions[] = {
    gText_FrameTypeRed,     // Previously Type 1
    gText_FrameTypeAqua,    // Previously Type 2
    gText_FrameTypeWhite,   // Previously Type 3
    gText_FrameTypeYellow,  // Previously Type 4
    gText_FrameTypeBlue,    // Previously Type 5
    gText_FrameTypeGreen,   // Previously Type 6
    gText_FrameTypePurple,  // Previously Type 7
    gText_FrameTypeOrange,  // Previously Type 8
    gText_FrameTypePink,    // Previously Type 9
    gText_FrameTypeTealGray // Previously Type 10
};

static const u8 *const sScrollBgsOptions[] = {
    gText_ScrollBgsOn,
    gText_ScrollBgsOff,
};

static const u8 *const sClockModeOptions[] = {
    gText_ClockMode12Hr,
    gText_ClockMode24Hr,
};

static const u8 *const sIVEVDisplayOptions[] = {
    gText_IVEVDisplayGraded,
    gText_IVEVDisplayExact,
};

#define TEXT_SPEED_OPTIONS_COUNT       ARRAY_COUNT(sTextSpeedOptions)
#define BATTLE_SCENE_OPTIONS_COUNT     ARRAY_COUNT(sBattleSceneOptions)
#define BATTLE_STYLE_OPTIONS_COUNT     ARRAY_COUNT(sBattleStyleOptions)
#define SOUND_OPTIONS_COUNT            ARRAY_COUNT(sSoundOptions)
#define BUTTON_MODE_OPTIONS_COUNT      ARRAY_COUNT(sButtonModeOptions)
#define FRAME_TYPE_OPTIONS_COUNT       ARRAY_COUNT(sFrameTypeOptions)
#define SCROLL_BGS_OPTIONS_COUNT       ARRAY_COUNT(sScrollBgsOptions)
#define CLOCK_MODE_OPTIONS_COUNT       ARRAY_COUNT(sClockModeOptions)
#define IV_EV_DISPLAY_OPTIONS_COUNT    ARRAY_COUNT(sIVEVDisplayOptions)
#define SHINY_ODDS_OPTIONS_COUNT       ARRAY_COUNT(sShinyOddsOptions)

// Menu draw and input functions
struct // MENU_GENERAL
{
    void (*drawChoices)(int selection, int y);
    int (*processInput)(int selection);
} static const sItemFunctionsGeneral[MENUITEM_GENERAL_COUNT] =
{
    [MENUITEM_GENERAL_TEXTSPEED]    = {DrawChoices_TextSpeed,   ProcessInput_TextSpeed},
    [MENUITEM_GENERAL_SOUND]        = {DrawChoices_Sound,       ProcessInput_Sound},
    [MENUITEM_GENERAL_BUTTONMODE]   = {DrawChoices_ButtonMode,  ProcessInput_ButtonMode},
    [MENUITEM_GENERAL_SHINYODDS]    = {DrawChoices_ShinyOdds,   ProcessInput_ShinyOdds},
};

struct // MENU_BATTLE
{
    void (*drawChoices)(int selection, int y);
    int (*processInput)(int selection);
} static const sItemFunctionsBattle[MENUITEM_BATTLE_COUNT] =
{
    [MENUITEM_BATTLE_BATTLESCENE]    = {DrawChoices_BattleScene, ProcessInput_BattleScene},
    [MENUITEM_BATTLE_BATTLESTYLE]    = {DrawChoices_BattleStyle, ProcessInput_BattleStyle},
};

struct // MENU_INTERFACE
{
    void (*drawChoices)(int selection, int y);
    int (*processInput)(int selection);
} static const sItemFunctionsInterface[MENUITEM_INTERFACE_COUNT] =
{
    [MENUITEM_INTERFACE_FRAMETYPE]   = {DrawChoices_FrameType,   ProcessInput_FrameType},
    [MENUITEM_INTERFACE_SCROLLBGS]   = {DrawChoices_ScrollBgs,   ProcessInput_ScrollBgs},
    [MENUITEM_INTERFACE_CLOCKMODE]   = {DrawChoices_ClockMode,   ProcessInput_ClockMode},
    [MENUITEM_INTERFACE_IVEVDISPLAY] = {DrawChoices_IVEVDisplay, ProcessInput_IVEVDisplay},
};


static const u8 *const sOptionMenuItemsNamesGeneral[MENUITEM_GENERAL_COUNT] =
{
    [MENUITEM_GENERAL_TEXTSPEED]   = gText_TextSpeed,
    [MENUITEM_GENERAL_SOUND]       = gText_Sound,
    [MENUITEM_GENERAL_BUTTONMODE]  = gText_ButtonMode,
    [MENUITEM_GENERAL_SHINYODDS]   = gText_ShinyOdds,
};

static const u8 *const sOptionMenuItemsNamesBattle[MENUITEM_BATTLE_COUNT] =
{
    [MENUITEM_BATTLE_BATTLESCENE] = gText_BattleScene,
    [MENUITEM_BATTLE_BATTLESTYLE] = gText_BattleStyle,
};

static const u8 *const sOptionMenuItemsNamesInterface[MENUITEM_INTERFACE_COUNT] =
{
    [MENUITEM_INTERFACE_FRAMETYPE]   = gText_Frame,
    [MENUITEM_INTERFACE_SCROLLBGS]   = gText_ScrollBgs,
    [MENUITEM_INTERFACE_CLOCKMODE]   = gText_ClockMode,
    [MENUITEM_INTERFACE_IVEVDISPLAY] = gText_IVEVs,
};


static const u8 *const OptionTextRight(u8 menuItem)
{
    switch (sOptions->submenu)
    {
        default:
        case MENU_GENERAL:
            return sOptionMenuItemsNamesGeneral[menuItem];
            break;
        case MENU_BATTLE:
            return sOptionMenuItemsNamesBattle[menuItem];
            break;
        case MENU_INTERFACE:
            return sOptionMenuItemsNamesInterface[menuItem];
            break;
    }
}

// Menu left side text conditions
static bool8 CheckConditions(int selection)
{
    switch (sOptions->submenu)
    {
        default:
        case MENU_GENERAL:
            switch(selection)
            {
                case MENUITEM_GENERAL_TEXTSPEED:
                    return TRUE;
                    break;
                case MENUITEM_GENERAL_SOUND:
                    return TRUE;
                    break;
                case MENUITEM_GENERAL_BUTTONMODE:
                    return TRUE;
                    break;
                default:
                case MENUITEM_GENERAL_COUNT:
                    return TRUE;
                    break;
                case MENUITEM_GENERAL_SHINYODDS:
                    return TRUE;
                    break;
            }
            break;
        case MENU_BATTLE:
            switch(selection)
            {
                case MENUITEM_BATTLE_BATTLESCENE:
                    return TRUE;
                    break;
                case MENUITEM_BATTLE_BATTLESTYLE:
                    return TRUE;
                    break;
                default:
                case MENUITEM_BATTLE_COUNT:
                    return TRUE;
                    break;
            }
            break;
        case MENU_INTERFACE:
            switch(selection)
            {
                case MENUITEM_INTERFACE_FRAMETYPE:
                    return TRUE;
                    break;
                case MENUITEM_INTERFACE_SCROLLBGS:
                    return TRUE;
                    break;
                case MENUITEM_INTERFACE_CLOCKMODE:
                    return TRUE;
                    break;
                case MENUITEM_INTERFACE_IVEVDISPLAY:
                    return TRUE;
                    break;
                default:
                case MENUITEM_INTERFACE_COUNT:
                    return TRUE;
                    break;
            }
            break;
    }
}


// Descriptions
static const u8 sText_Empty[]                   = _("");
static const u8 sText_Desc_TextSpeed[]          = _("Choose one of the 3 text-display\nspeeds.");
static const u8 sText_Desc_Sound[]              = _("Changes the sound mode. Mono is\nrecommended for original hardware.");
static const u8 sText_Desc_ButtonMode[]         = _("Changes the functions of the\nbuttons.");
static const u8 sText_Desc_ShinyOdds[]          = _("Adjust the odds of encountering\nshiny Pokémon.");

static const u8 sText_Desc_BattleScene[]     = _("Chooses whether to show battle\nanimations.");
static const u8 sText_Desc_BattleStyle[]  = _("Set: No switching after KO.\nShift: Switch prompt after KO.");

static const u8 sText_Desc_FrameType[]          = _("Choose the frame surrounding the\nwindows.");
static const u8 sText_Desc_ScrollBgs[]          = _("Toggles the scrolling of the UI\nmenu backgrounds.");
static const u8 sText_Desc_ClockMode[]          = _("Changes the clock display between\n12-hour and 24-hour format.");
static const u8 sText_Desc_IVEVDisplay[]        = _("Choose how IVs and EVs are displayed\nin the summary screen.");

static const u8 *const sOptionMenuItemDescriptionsGeneral[MENUITEM_GENERAL_COUNT][1] =
{
    [MENUITEM_GENERAL_TEXTSPEED]   = {sText_Desc_TextSpeed},
    [MENUITEM_GENERAL_SOUND]       = {sText_Desc_Sound},
    [MENUITEM_GENERAL_BUTTONMODE]  = {sText_Desc_ButtonMode},
    [MENUITEM_GENERAL_SHINYODDS]   = {sText_Desc_ShinyOdds},
};

static const u8 *const sOptionMenuItemDescriptionsBattle[MENUITEM_BATTLE_COUNT][1] =
{
    [MENUITEM_BATTLE_BATTLESCENE] = {sText_Desc_BattleScene},
    [MENUITEM_BATTLE_BATTLESTYLE] = {sText_Desc_BattleStyle},
};

static const u8 *const sOptionMenuItemDescriptionsInterface[MENUITEM_INTERFACE_COUNT][1] =
{
    [MENUITEM_INTERFACE_FRAMETYPE]   = {sText_Desc_FrameType,},
    [MENUITEM_INTERFACE_SCROLLBGS]   = {sText_Desc_ScrollBgs},
    [MENUITEM_INTERFACE_CLOCKMODE]   = {sText_Desc_ClockMode},
    [MENUITEM_INTERFACE_IVEVDISPLAY] = {sText_Desc_IVEVDisplay},
};


static const u8 *const OptionTextDescription(void)
{
    u8 menuItem = sOptions->menuCursor[sOptions->submenu];
    u8 selection;

    switch (sOptions->submenu)
    {
        default:
        case MENU_GENERAL:
            selection = 0;
            return sOptionMenuItemDescriptionsGeneral[menuItem][selection];
            break;
        case MENU_BATTLE:
            selection = 0;
            return sOptionMenuItemDescriptionsBattle[menuItem][selection];
            break;
        case MENU_INTERFACE:
            selection = 0;
            return sOptionMenuItemDescriptionsInterface[menuItem][selection];
            break;
    }
}

static u8 MenuItemCount(void)
{
    switch (sOptions->submenu)
    {
        default:
        case MENU_GENERAL:
            return MENUITEM_GENERAL_COUNT;
            break;
        case MENU_BATTLE:
            return MENUITEM_BATTLE_COUNT;
            break;
        case MENU_INTERFACE:
            return MENUITEM_INTERFACE_COUNT;
            break;
    }
}

// Main code
static void MainCB2(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    DoScheduledBgTilemapCopiesToVram();
    UpdatePaletteFade();
}

static void VBlankCB(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();

    if (gSaveBlock4Ptr->optionsScrollBgs == OPTIONS_SCROLL_BGS_ON)
    {
        ChangeBgX(3, 64, BG_COORD_ADD);
        ChangeBgY(3, 64, BG_COORD_ADD);
    }
}

static const u8 sText_TopBar_General[]      = _("General Options");
static const u8 sText_TopBar_Battle[]       = _("Battle Options");
static const u8 sText_TopBar_Interface[]    = _("Interface Options");
static const u8 sText_TopBar_Left[]         = _("{L_BUTTON}");
static const u8 sText_TopBar_Right[]        = _("{R_BUTTON}");

static void DrawTopBarText(void)
{
    const u8 color[3] = {0, 2, 10};

    FillWindowPixelBuffer(WIN_TOPBAR, PIXEL_FILL(0));
    switch (sOptions->submenu)
    {
        case MENU_GENERAL:
            AddTextPrinterParameterized3(WIN_TOPBAR, FONT_SMALL, 86, 1, color, 0, sText_TopBar_General);
            AddTextPrinterParameterized3(WIN_TOPBAR, FONT_SMALL, 2, 1, color, 0, sText_TopBar_Left);
            AddTextPrinterParameterized3(WIN_TOPBAR, FONT_SMALL, 221, 1, color, 0, sText_TopBar_Right);
            break;
        case MENU_BATTLE:
            AddTextPrinterParameterized3(WIN_TOPBAR, FONT_SMALL, 88, 1, color, 0, sText_TopBar_Battle);
            AddTextPrinterParameterized3(WIN_TOPBAR, FONT_SMALL, 2, 1, color, 0, sText_TopBar_Left);
            AddTextPrinterParameterized3(WIN_TOPBAR, FONT_SMALL, 221, 1, color, 0, sText_TopBar_Right);
            break;
        case MENU_INTERFACE:
            AddTextPrinterParameterized3(WIN_TOPBAR, FONT_SMALL, 80, 1, color, 0, sText_TopBar_Interface);
            AddTextPrinterParameterized3(WIN_TOPBAR, FONT_SMALL, 2, 1, color, 0, sText_TopBar_Left);
            AddTextPrinterParameterized3(WIN_TOPBAR, FONT_SMALL, 221, 1, color, 0, sText_TopBar_Right);
            break;
    }
    PutWindowTilemap(WIN_TOPBAR);
    CopyWindowToVram(WIN_TOPBAR, COPYWIN_FULL);
}

static void DrawOptionMenuTexts(void) //left side text
{
    u8 i;

    FillWindowPixelBuffer(WIN_OPTIONS, PIXEL_FILL(0));
    for (i = 0; i < MenuItemCount(); i++)
        DrawLeftSideOptionText(i, (i * Y_DIFF) + 1);
    CopyWindowToVram(WIN_OPTIONS, COPYWIN_FULL);
}

static void DrawDescriptionText(void)
{
    u8 color[3];
    color[0] = 0;
    color[1] = 2;
    color[2] = 10;
        
    FillWindowPixelBuffer(WIN_DESCRIPTION, PIXEL_FILL(1));
    AddTextPrinterParameterized4(WIN_DESCRIPTION, FONT_NORMAL, 8, 1, 0, 0, color, TEXT_SKIP_DRAW, OptionTextDescription());
    CopyWindowToVram(WIN_DESCRIPTION, COPYWIN_FULL);
}

static void DrawLeftSideOptionText(int selection, int y)
{
    u8 color[3];

    color[0] = 0;
    color[1] = 2;
    color[2] = 10;

    AddTextPrinterParameterized4(WIN_OPTIONS, FONT_NORMAL, 8, y, 0, 0, color, TEXT_SKIP_DRAW, OptionTextRight(selection));
}

static void DrawRightSideChoiceText(const u8 *text, int x, int y, bool8 choosen, bool8 active)
{
    u8 colorSelected[3];
    u8 color[3];

    color[0] = 0;
    color[1] = 2;
    color[2] = 10;

    colorSelected[0] = 0;
    colorSelected[1] = 4;
    colorSelected[2] = 10;


    if (choosen)
        AddTextPrinterParameterized4(WIN_OPTIONS, FONT_NORMAL, x, y, 0, 0, colorSelected, TEXT_SKIP_DRAW, text);
    else
        AddTextPrinterParameterized4(WIN_OPTIONS, FONT_NORMAL, x, y, 0, 0, color, TEXT_SKIP_DRAW, text);
}

static void DrawChoices(u32 id, int y) //right side draw function
{
    switch (sOptions->submenu)
    {
        case MENU_GENERAL:
            if (sItemFunctionsGeneral[id].drawChoices != NULL)
                sItemFunctionsGeneral[id].drawChoices(sOptions->selGeneral[id], y);
            break;
        case MENU_BATTLE:
            if (sItemFunctionsBattle[id].drawChoices != NULL)
                sItemFunctionsBattle[id].drawChoices(sOptions->selBattle[id], y);
            break;
        case MENU_INTERFACE:
            if (sItemFunctionsInterface[id].drawChoices != NULL)
                sItemFunctionsInterface[id].drawChoices(sOptions->selInterface[id], y);
            break;
    }
}

static void HighlightOptionMenuItem(void)
{
    int cursor = sOptions->visibleCursor[sOptions->submenu];

    SetGpuReg(REG_OFFSET_WIN0H, WIN_RANGE(13, 231));
    SetGpuReg(REG_OFFSET_WIN0V, WIN_RANGE(cursor * Y_DIFF + 24, cursor * Y_DIFF + 40));
}

static bool8 OptionsMenu_LoadGraphics(void) // Load all the tilesets, tilemaps, spritesheets, and palettes
{
    switch (sOptions->gfxLoadState)
    {
    case 0:
        ResetTempTileDataBuffers();
        DecompressAndCopyTileDataToVram(2, sOptionsMenuTiles, 0, 0, 0);
        sOptions->gfxLoadState++;
        break;
    case 1:
        if (FreeTempTileDataBuffersIfPossible() != TRUE)
        {
            DecompressDataWithHeaderWram(sOptionsMenuTilemap, sBg2TilemapBuffer);
            sOptions->gfxLoadState++;
        }
        break;
    case 2:
        ResetTempTileDataBuffers();
        DecompressAndCopyTileDataToVram(3, sScrollBgTiles, 0, 0, 0);
        sOptions->gfxLoadState++;
        break;
    case 3:
        if (FreeTempTileDataBuffersIfPossible() != TRUE)
        {
            DecompressDataWithHeaderWram(sScrollBgTilemap, sBg3TilemapBuffer);
            sOptions->gfxLoadState++;
        }
        break;
    case 4:
        LoadPalette(sOptionsMenuPalette, PLTT_ID(2), PLTT_SIZE_4BPP);
        LoadPalette(sScrollBgPalette, PLTT_ID(3), PLTT_SIZE_4BPP);
        sOptions->gfxLoadState++;
        break;
    default:
        sOptions->gfxLoadState = 0;
        return TRUE;
    }
    return FALSE;
}

void CB2_InitUIOptionMenu(void)
{
    u32 i;
    switch (gMain.state)
    {
    default:
    case 0:
        SetVBlankHBlankCallbacksToNull();
        ClearScheduledBgCopiesToVram();
        ResetVramOamAndBgCntRegs();
        sOptions = AllocZeroed(sizeof(*sOptions));
        FreeAllSpritePalettes();
        ResetTasks();
        ResetSpriteData();
        gMain.state++;
        break;
    case 1:
        DmaClearLarge16(3, (void *)(VRAM), VRAM_SIZE, 0x1000);
        DmaClear32(3, OAM, OAM_SIZE);
        DmaClear16(3, PLTT, PLTT_SIZE);
        ResetBgsAndClearDma3BusyFlags(0);
        ResetBgPositions();
        
        DeactivateAllTextPrinters();
        SetGpuReg(REG_OFFSET_WIN0H, 0);
        SetGpuReg(REG_OFFSET_WIN0V, 0);
        SetGpuReg(REG_OFFSET_WININ, WININ_WIN0_BG_ALL | WININ_WIN0_OBJ);
        SetGpuReg(REG_OFFSET_WINOUT, WINOUT_WIN01_BG_ALL | WINOUT_WIN01_OBJ | WINOUT_WIN01_CLR);
        SetGpuReg(REG_OFFSET_BLDCNT, BLDCNT_EFFECT_DARKEN | BLDCNT_TGT1_BG0 | BLDCNT_TGT1_BG2);
        SetGpuReg(REG_OFFSET_BLDALPHA, 0);
        SetGpuReg(REG_OFFSET_BLDY, 4);
        SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_WIN0_ON | DISPCNT_WIN1_ON);
        
        ResetAllBgsCoordinates();
        ResetBgsAndClearDma3BusyFlags(0);
        InitBgsFromTemplates(0, sOptionMenuBgTemplates, NELEMS(sOptionMenuBgTemplates));
        InitWindows(sOptionMenuWinTemplates);

        sBg2TilemapBuffer = Alloc(0x800);
        memset(sBg2TilemapBuffer, 0, 0x800);
        SetBgTilemapBuffer(2, sBg2TilemapBuffer);
        ScheduleBgCopyTilemapToVram(2);

        sBg3TilemapBuffer = Alloc(0x800);
        memset(sBg3TilemapBuffer, 0, 0x800);
        SetBgTilemapBuffer(3, sBg3TilemapBuffer);
        ScheduleBgCopyTilemapToVram(3);
        gMain.state++;
        break;
    case 2:
        ResetPaletteFade();
        ScanlineEffect_Stop();
        gMain.state++;
        sOptions->gfxLoadState = 0;
        break;
    case 3:
        if (OptionsMenu_LoadGraphics() == TRUE)
        {
            gMain.state++;
            LoadBgTiles(1, GetWindowFrameTilesPal(gSaveBlock4Ptr->optionsWindowFrameType)->tiles, 0x120, 0x1A2);
        }
        break;
    case 4:
        LoadPalette(sOptionMenuBg_Pal, PLTT_ID(1), PLTT_SIZE_4BPP);
        LoadPalette(GetWindowFrameTilesPal(gSaveBlock4Ptr->optionsWindowFrameType)->pal, 0x70, 0x20);
        gMain.state++;
        break;
    case 5:
        LoadPalette(sOptionMenuText_Pal, PLTT_ID(0), PLTT_SIZE_4BPP);
        gMain.state++;
        break;
    case 6:
        sOptions->selGeneral[MENUITEM_GENERAL_TEXTSPEED]   = gSaveBlock4Ptr->optionsTextSpeed;
        sOptions->selGeneral[MENUITEM_GENERAL_SOUND]       = gSaveBlock4Ptr->optionsSound;
        sOptions->selGeneral[MENUITEM_GENERAL_BUTTONMODE]  = gSaveBlock4Ptr->optionsButtonMode;
        sOptions->selGeneral[MENUITEM_GENERAL_SHINYODDS]   = gSaveBlock4Ptr->optionsShinyOdds;

        sOptions->selBattle[MENUITEM_BATTLE_BATTLESCENE] = gSaveBlock4Ptr->optionsBattleScene;
        sOptions->selBattle[MENUITEM_BATTLE_BATTLESTYLE] = gSaveBlock4Ptr->optionsBattleStyle;

        sOptions->selInterface[MENUITEM_INTERFACE_FRAMETYPE]   = gSaveBlock4Ptr->optionsWindowFrameType;
        sOptions->selInterface[MENUITEM_INTERFACE_SCROLLBGS]   = gSaveBlock4Ptr->optionsScrollBgs;
        sOptions->selInterface[MENUITEM_INTERFACE_CLOCKMODE]   = gSaveBlock4Ptr->optionsClockMode;
        sOptions->selInterface[MENUITEM_INTERFACE_IVEVDISPLAY] = gSaveBlock4Ptr->optionsIVEVDisplay;

        sOptions->submenu = MENU_GENERAL;

        gMain.state++;
        break;
    case 7:
        PutWindowTilemap(WIN_TOPBAR);
        DrawTopBarText();
        gMain.state++;
        break;
    case 8:
        PutWindowTilemap(WIN_DESCRIPTION);
        DrawDescriptionText();
        gMain.state++;
        break;
    case 9:
        PutWindowTilemap(WIN_OPTIONS);
        DrawOptionMenuTexts();
        gMain.state++;
        break;
    case 10:
        CreateTask(Task_OptionMenuFadeIn, 0);
        
        if (MenuItemCount() <= OPTIONS_ON_SCREEN) // Draw the scrolling arrows based on options in the menu
        {
            if (sOptions->arrowTaskId != TASK_NONE)
            {
                sOptions->arrowTaskId = TASK_NONE;
            }
        }
        else
        {
            if (sOptions->arrowTaskId == TASK_NONE)
                sOptions->arrowTaskId  = AddScrollIndicatorArrowPairParameterized(SCROLL_ARROW_UP, 240 / 2, 20, 110, MenuItemCount() - 1, 110, 110, 0);

        }

        for (i = 0; i < min(OPTIONS_ON_SCREEN, MenuItemCount()); i++)
            DrawChoices(i, i * Y_DIFF);

        HighlightOptionMenuItem();

        CopyWindowToVram(WIN_OPTIONS, COPYWIN_FULL);
        gMain.state++;
        break;
    case 11:
        DrawBgWindowFrames();
        gMain.state++;
        break;
    case 12:
        ShowBg(0);
        ShowBg(1);
        ShowBg(2);
        ShowBg(3);
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0x10, 0, RGB_BLACK);
        SetVBlankCallback(VBlankCB);
        SetMainCallback2(MainCB2);
        return;
    }
}

static void Task_OptionMenuFadeIn(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        gTasks[taskId].func = Task_OptionMenuProcessInput;
        SetGpuReg(REG_OFFSET_WIN0H, 0);
        SetGpuReg(REG_OFFSET_WIN0V, 0);
        SetGpuReg(REG_OFFSET_WININ, WININ_WIN0_BG_ALL | WININ_WIN0_OBJ);
        SetGpuReg(REG_OFFSET_WINOUT, WINOUT_WIN01_BG_ALL | WINOUT_WIN01_OBJ | WINOUT_WIN01_CLR);
        SetGpuReg(REG_OFFSET_BLDCNT, BLDCNT_EFFECT_DARKEN | BLDCNT_TGT1_BG0 | BLDCNT_TGT1_BG2);
        SetGpuReg(REG_OFFSET_BLDALPHA, 0);
        SetGpuReg(REG_OFFSET_BLDY, 4);
        SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_WIN0_ON | DISPCNT_WIN1_ON | DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP);
        ShowBg(0);
        ShowBg(1);
        ShowBg(2);
        ShowBg(3);
        HighlightOptionMenuItem();
        return;
    }
}

static void Task_OptionMenuProcessInput(u8 taskId)
{
    u8 optionsToDraw = min(OPTIONS_ON_SCREEN , MenuItemCount());

    if (JOY_NEW(B_BUTTON))
    {
        gTasks[taskId].func = Task_OptionMenuSave;
    }
    else if (JOY_NEW(DPAD_UP))
    {
        if (sOptions->visibleCursor[sOptions->submenu] == NUM_OPTIONS_FROM_BORDER) // don't advance visible cursor until scrolled to the bottom
        {
            if (--sOptions->menuCursor[sOptions->submenu] == 0)
                sOptions->visibleCursor[sOptions->submenu]--;
            else
                ScrollMenu(1);
        }
        else
        {
            if (--sOptions->menuCursor[sOptions->submenu] < 0) // Scroll all the way to the bottom.
            {
                sOptions->visibleCursor[sOptions->submenu] = sOptions->menuCursor[sOptions->submenu] = optionsToDraw-2;
                ScrollAll(0);
                sOptions->visibleCursor[sOptions->submenu] = optionsToDraw-1;
                sOptions->menuCursor[sOptions->submenu] = MenuItemCount() - 1;
            }
            else
            {
                sOptions->visibleCursor[sOptions->submenu]--;
            }
        }
        HighlightOptionMenuItem();
        DrawDescriptionText();
    }
    else if (JOY_NEW(DPAD_DOWN))
    {
        if (sOptions->visibleCursor[sOptions->submenu] == optionsToDraw-2) // don't advance visible cursor until scrolled to the bottom
        {
            if (++sOptions->menuCursor[sOptions->submenu] == MenuItemCount() - 1)
                sOptions->visibleCursor[sOptions->submenu]++;
            else
                ScrollMenu(0);
        }
        else
        {
            if (++sOptions->menuCursor[sOptions->submenu] >= MenuItemCount()-1) // Scroll all the way to the top.
            {
                sOptions->visibleCursor[sOptions->submenu] = optionsToDraw-2;
                sOptions->menuCursor[sOptions->submenu] = MenuItemCount() - optionsToDraw-1;
                ScrollAll(1);
                sOptions->visibleCursor[sOptions->submenu] = sOptions->menuCursor[sOptions->submenu] = 0;
            }
            else
            {
                sOptions->visibleCursor[sOptions->submenu]++;
            }
        }
        HighlightOptionMenuItem();
        DrawDescriptionText();
    }
    else if (JOY_NEW(DPAD_LEFT | DPAD_RIGHT))
    {
        if (sOptions->submenu == MENU_GENERAL)
        {
            int cursor = sOptions->menuCursor[sOptions->submenu];
            u8 previousOption = sOptions->selGeneral[cursor];
            if (CheckConditions(cursor))
            {
                if (sItemFunctionsGeneral[cursor].processInput != NULL)
                {
                    sOptions->selGeneral[cursor] = sItemFunctionsGeneral[cursor].processInput(previousOption);
                    ReDrawAll();
                    DrawDescriptionText();
                }

                if (previousOption != sOptions->selGeneral[cursor])
                    DrawChoices(cursor, sOptions->visibleCursor[sOptions->submenu] * Y_DIFF);
            }
        }
        else if (sOptions->submenu == MENU_BATTLE)
        {
            int cursor = sOptions->menuCursor[sOptions->submenu];
            u8 previousOption = sOptions->selBattle[cursor];
            if (CheckConditions(cursor))
            {
                if (sItemFunctionsBattle[cursor].processInput != NULL)
                {
                    sOptions->selBattle[cursor] = sItemFunctionsBattle[cursor].processInput(previousOption);
                    ReDrawAll();
                    DrawDescriptionText();
                }

                if (previousOption != sOptions->selBattle[cursor])
                    DrawChoices(cursor, sOptions->visibleCursor[sOptions->submenu] * Y_DIFF);
            }
        }
        else if (sOptions->submenu == MENU_INTERFACE)
        {
            int cursor = sOptions->menuCursor[sOptions->submenu];
            u8 previousOption = sOptions->selInterface[cursor];
            if (CheckConditions(cursor))
            {
                if (sItemFunctionsInterface[cursor].processInput != NULL)
                {
                    sOptions->selInterface[cursor] = sItemFunctionsInterface[cursor].processInput(previousOption);
                    ReDrawAll();
                    DrawDescriptionText();
                }

                if (previousOption != sOptions->selInterface[cursor])
                    DrawChoices(cursor, sOptions->visibleCursor[sOptions->submenu] * Y_DIFF);
            }
        }
    }
    else if (JOY_NEW(R_BUTTON))
    {
        if (sOptions->submenu != MENU_INTERFACE)
            sOptions->submenu++;
        else
            sOptions->submenu = MENU_GENERAL;

        DrawTopBarText();
        ReDrawAll();
        HighlightOptionMenuItem();
        DrawDescriptionText();
    }
    else if (JOY_NEW(L_BUTTON))
    {
        if (sOptions->submenu != MENU_GENERAL)
            sOptions->submenu--;
        else
            sOptions->submenu = MENU_INTERFACE;
        
        DrawTopBarText();
        ReDrawAll();
        HighlightOptionMenuItem();
        DrawDescriptionText();
    }
}

static void Task_OptionMenuSave(u8 taskId)
{
    gSaveBlock4Ptr->optionsTextSpeed        = sOptions->selGeneral[MENUITEM_GENERAL_TEXTSPEED];
    gSaveBlock4Ptr->optionsSound            = sOptions->selGeneral[MENUITEM_GENERAL_SOUND];
    gSaveBlock4Ptr->optionsButtonMode       = sOptions->selGeneral[MENUITEM_GENERAL_BUTTONMODE];
    gSaveBlock4Ptr->optionsShinyOdds        = sOptions->selGeneral[MENUITEM_GENERAL_SHINYODDS];

    gSaveBlock4Ptr->optionsBattleScene      = sOptions->selBattle[MENUITEM_BATTLE_BATTLESCENE];
    gSaveBlock4Ptr->optionsBattleStyle      = sOptions->selBattle[MENUITEM_BATTLE_BATTLESTYLE];

    gSaveBlock4Ptr->optionsWindowFrameType  = sOptions->selInterface[MENUITEM_INTERFACE_FRAMETYPE];
    gSaveBlock4Ptr->optionsScrollBgs        = sOptions->selInterface[MENUITEM_INTERFACE_SCROLLBGS];
    gSaveBlock4Ptr->optionsClockMode        = sOptions->selInterface[MENUITEM_INTERFACE_CLOCKMODE];
    gSaveBlock4Ptr->optionsIVEVDisplay      = sOptions->selInterface[MENUITEM_INTERFACE_IVEVDISPLAY];

    BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 0x10, RGB_BLACK);
    gTasks[taskId].func = Task_OptionMenuFadeOut;
}

#define try_free(ptr) ({        \
    void ** ptr__ = (void **)&(ptr);   \
    if (*ptr__ != NULL)                \
        Free(*ptr__);                  \
})

static void Task_OptionMenuFadeOut(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        DestroyTask(taskId);
        FreeAllWindowBuffers();
        FREE_AND_SET_NULL(sOptions);
        try_free(sBg2TilemapBuffer);
        try_free(sBg3TilemapBuffer);
        SetGpuReg(REG_OFFSET_WIN0H, 0);
        SetGpuReg(REG_OFFSET_WIN0V, 0);
        SetGpuReg(REG_OFFSET_WININ, 0);
        SetGpuReg(REG_OFFSET_WINOUT, 0);
        SetGpuReg(REG_OFFSET_BLDCNT, 0);
        SetGpuReg(REG_OFFSET_BLDALPHA, 0);
        SetGpuReg(REG_OFFSET_BLDY, 4);
        SetGpuReg(REG_OFFSET_DISPCNT, 0);
        HideBg(2);
        HideBg(3);
        SetMainCallback2(gMain.savedCallback);
    }
}

static void ScrollMenu(int direction)
{
    int menuItem, pos;
    u8 optionsToDraw = min(OPTIONS_ON_SCREEN, MenuItemCount());

    if (direction == 0) // scroll down
        menuItem = sOptions->menuCursor[sOptions->submenu] + NUM_OPTIONS_FROM_BORDER, pos = optionsToDraw - 1;
    else
        menuItem = sOptions->menuCursor[sOptions->submenu] - NUM_OPTIONS_FROM_BORDER, pos = 0;

    // Hide one
    ScrollWindow(WIN_OPTIONS, direction, Y_DIFF, PIXEL_FILL(0));
    // Show one
    FillWindowPixelRect(WIN_OPTIONS, PIXEL_FILL(0), 0, Y_DIFF * pos, 26 * 8, Y_DIFF);
    // Print
    DrawChoices(menuItem, pos * Y_DIFF);
    DrawLeftSideOptionText(menuItem, (pos * Y_DIFF) + 1);
    CopyWindowToVram(WIN_OPTIONS, COPYWIN_GFX);
}

static void ScrollAll(int direction) // to bottom or top
{
    int i, y, menuItem, pos;
    int scrollCount;
    u8 optionsToDraw = min(OPTIONS_ON_SCREEN, MenuItemCount());

    scrollCount = MenuItemCount() - optionsToDraw;

    // Move items up/down
    ScrollWindow(WIN_OPTIONS, direction, Y_DIFF * scrollCount, PIXEL_FILL(1));

    // Clear moved items
    if (direction == 0)
    {
        y = optionsToDraw - scrollCount;
        if (y < 0)
            y = optionsToDraw;
        y *= Y_DIFF;
    }
    else
    {
        y = 0;
    }

    FillWindowPixelRect(WIN_OPTIONS, PIXEL_FILL(0), 0, y, 26 * 8, Y_DIFF * scrollCount);
    // Print new texts
    for (i = 0; i < scrollCount; i++)
    {
        if (direction == 0) // From top to bottom
            menuItem = MenuItemCount() - 1 - i, pos = optionsToDraw - 1 - i;
        else // From bottom to top
            menuItem = i, pos = i;
        DrawChoices(menuItem, pos * Y_DIFF);
        DrawLeftSideOptionText(menuItem, (pos * Y_DIFF) + 1);
    }
    CopyWindowToVram(WIN_OPTIONS, COPYWIN_GFX);
}

static int ProcessInput_TextSpeed(int selection)
{
    if (JOY_NEW(DPAD_LEFT) && selection > 0)
    {
        selection--;
    }
    else if (JOY_NEW(DPAD_RIGHT) && selection < TEXT_SPEED_OPTIONS_COUNT - 1)
    {
        selection++;
    }

    return selection;
}

static int ProcessInput_Sound(int selection)
{
    if (JOY_NEW(DPAD_LEFT) && selection > 0)
    {
        selection--;
        SetPokemonCryStereo(selection);
    }
    else if (JOY_NEW(DPAD_RIGHT) && selection < SOUND_OPTIONS_COUNT - 1)
    {
        selection++;
        SetPokemonCryStereo(selection);
    }

    return selection;
}

static int ProcessInput_ButtonMode(int selection)
{
    if (JOY_NEW(DPAD_LEFT) && selection > 0)
    {
        selection--;
    }
    else if (JOY_NEW(DPAD_RIGHT) && selection < BUTTON_MODE_OPTIONS_COUNT - 1)
    {
        selection++;
    }

    return selection;
}

static int ProcessInput_ShinyOdds(int selection)
{
    if (JOY_NEW(DPAD_LEFT) && selection > 0)
    {
        selection--;
    }
    else if (JOY_NEW(DPAD_RIGHT) && selection < SHINY_ODDS_OPTIONS_COUNT - 1)
    {
        selection++;
    }

    return selection;
}

static int ProcessInput_BattleScene(int selection)
{
    if (JOY_NEW(DPAD_LEFT) && selection > 0)
    {
        selection--;
    }
    else if (JOY_NEW(DPAD_RIGHT) && selection < BATTLE_SCENE_OPTIONS_COUNT - 1)
    {
        selection++;
    }

    return selection;
}

static int ProcessInput_BattleStyle(int selection)
{
    if (JOY_NEW(DPAD_LEFT) && selection > 0)
    {
        selection--;
    }
    else if (JOY_NEW(DPAD_RIGHT) && selection < BATTLE_STYLE_OPTIONS_COUNT - 1)
    {
        selection++;
    }

    return selection;
}

static int ProcessInput_FrameType(int selection)
{
    if (JOY_NEW(DPAD_RIGHT))
    {
        if (selection < WINDOW_FRAMES_COUNT - 1)
            selection++;

        LoadBgTiles(1, GetWindowFrameTilesPal(selection)->tiles, 0x120, 0x1A2);
        LoadPalette(GetWindowFrameTilesPal(selection)->pal, 0x70, 0x20);
    }
    if (JOY_NEW(DPAD_LEFT))
    {
        if (selection != 0)
            selection--;

        LoadBgTiles(1, GetWindowFrameTilesPal(selection)->tiles, 0x120, 0x1A2);
        LoadPalette(GetWindowFrameTilesPal(selection)->pal, 0x70, 0x20);
    }
    return selection;
}

static int ProcessInput_ScrollBgs(int selection)
{
    if (JOY_NEW(DPAD_LEFT) && selection > 0)
    {
        selection--;
    }
    else if (JOY_NEW(DPAD_RIGHT) && selection < SCROLL_BGS_OPTIONS_COUNT - 1)
    {
        selection++;
    }
    
    gSaveBlock4Ptr->optionsScrollBgs = selection;
    return selection;
}

static int ProcessInput_ClockMode(int selection)
{
    if (JOY_NEW(DPAD_LEFT) && selection > 0)
    {
        selection--;
    }
    else if (JOY_NEW(DPAD_RIGHT) && selection < CLOCK_MODE_OPTIONS_COUNT - 1)
    {
        selection++;
    }

    return selection;
}

static int ProcessInput_IVEVDisplay(int selection)
{
    if (JOY_NEW(DPAD_LEFT) && selection > 0)
    {
        selection--;
    }
    else if (JOY_NEW(DPAD_RIGHT) && selection < IV_EV_DISPLAY_OPTIONS_COUNT - 1)
    {
        selection++;
    }

    return selection;
}


static void DrawOptionMenuChoice(const u8 *text, u8 x, u8 y, u8 style, bool8 active)
{
    bool8 choosen = FALSE;
    if (style != 0)
        choosen = TRUE;

    DrawRightSideChoiceText(text, x, y+1, choosen, active);
}

static void ReDrawAll(void)
{
    u8 menuItem = sOptions->menuCursor[sOptions->submenu] - sOptions->visibleCursor[sOptions->submenu];
    u8 i;
    u8 optionsToDraw = min(OPTIONS_ON_SCREEN, MenuItemCount());

    if (MenuItemCount() <= OPTIONS_ON_SCREEN) // Draw or delete the scrolling arrows based on options in the menu
    {
        if (sOptions->arrowTaskId != TASK_NONE)
        {
            RemoveScrollIndicatorArrowPair(sOptions->arrowTaskId);
            sOptions->arrowTaskId = TASK_NONE;
        }
    }
    else
    {
        if (sOptions->arrowTaskId == TASK_NONE)
            sOptions->arrowTaskId  = AddScrollIndicatorArrowPairParameterized(SCROLL_ARROW_UP, 240 / 2, 20, 110, MenuItemCount() - 1, 110, 110, 0);

    }

    FillWindowPixelBuffer(WIN_OPTIONS, PIXEL_FILL(0));
    for (i = 0; i < optionsToDraw; i++)
    {
        DrawChoices(menuItem+i, i * Y_DIFF);
        DrawLeftSideOptionText(menuItem+i, (i * Y_DIFF) + 1);
    }
    CopyWindowToVram(WIN_OPTIONS, COPYWIN_GFX);
}

// Process Input functions
static void DrawChoices_TextSpeed(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_GENERAL_TEXTSPEED);
    const u8 *leftArrow = gText_DPadLeft;
    const u8 *rightArrow = gText_DPadRight;
    const u8 *choiceText;
    s32 xLeft, xChoice, xRight;
    s32 leftWidth, choiceWidth;

    if (selection >= TEXT_SPEED_OPTIONS_COUNT)
        selection = 0;

    choiceText = sTextSpeedOptions[selection % TEXT_SPEED_OPTIONS_COUNT];

    FillWindowPixelRect(WIN_OPTIONS, PIXEL_FILL(1), 104, y, 96, 16);

    leftWidth = GetStringWidth(FONT_NORMAL, leftArrow, 0);
    choiceWidth = GetStringWidth(FONT_NORMAL, choiceText, 0);

    xChoice = 152 - (choiceWidth / 2);
    xLeft = xChoice - leftWidth - 4;
    xRight = xChoice + choiceWidth + 4;

    if (selection > 0)
        AddTextPrinterParameterized(WIN_OPTIONS, FONT_NORMAL, leftArrow, xLeft, y, TEXT_SKIP_DRAW, NULL);

    DrawOptionMenuChoice(choiceText, xChoice, y, 1, active);

    if (selection < TEXT_SPEED_OPTIONS_COUNT - 1)
        AddTextPrinterParameterized(WIN_OPTIONS, FONT_NORMAL, rightArrow, xRight, y, TEXT_SKIP_DRAW, NULL);
}

static void DrawChoices_Sound(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_GENERAL_SOUND);
    const u8 *choiceText = sSoundOptions[selection];
    s32 choiceWidth = GetStringWidth(FONT_NORMAL, choiceText, 0);
    s32 xChoice = 152 - (choiceWidth / 2);
    s32 xLeft = xChoice - GetStringWidth(FONT_NORMAL, gText_DPadLeft, 0) - 4;
    s32 xRight = xChoice + choiceWidth + 4;

    FillWindowPixelRect(WIN_OPTIONS, PIXEL_FILL(1), 104, y, 96, 16);

    if (selection > 0)
        AddTextPrinterParameterized(WIN_OPTIONS, FONT_NORMAL, gText_DPadLeft, xLeft, y, TEXT_SKIP_DRAW, NULL);

    DrawOptionMenuChoice(choiceText, xChoice, y, 1, active);

    if (selection < SOUND_OPTIONS_COUNT - 1)
        AddTextPrinterParameterized(WIN_OPTIONS, FONT_NORMAL, gText_DPadRight, xRight, y, TEXT_SKIP_DRAW, NULL);
}

static void DrawChoices_ButtonMode(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_GENERAL_BUTTONMODE);
    const u8 *choiceText = sButtonModeOptions[selection];
    s32 choiceWidth = GetStringWidth(FONT_NORMAL, choiceText, 0);
    s32 xChoice = 152 - (choiceWidth / 2);
    s32 xLeft = xChoice - GetStringWidth(FONT_NORMAL, gText_DPadLeft, 0) - 4;
    s32 xRight = xChoice + choiceWidth + 4;

    FillWindowPixelRect(WIN_OPTIONS, PIXEL_FILL(1), 104, y, 96, 16);

    if (selection > 0)
        AddTextPrinterParameterized(WIN_OPTIONS, FONT_NORMAL, gText_DPadLeft, xLeft, y, TEXT_SKIP_DRAW, NULL);

    DrawOptionMenuChoice(choiceText, xChoice, y, 1, active);

    if (selection < BUTTON_MODE_OPTIONS_COUNT - 1)
        AddTextPrinterParameterized(WIN_OPTIONS, FONT_NORMAL, gText_DPadRight, xRight, y, TEXT_SKIP_DRAW, NULL);
}

static void DrawChoices_ShinyOdds(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_GENERAL_SHINYODDS);
    const u8 *choiceText = sShinyOddsOptions[selection];
    s32 choiceWidth = GetStringWidth(FONT_NORMAL, choiceText, 0);
    s32 xChoice = 152 - (choiceWidth / 2);
    s32 xLeft = xChoice - GetStringWidth(FONT_NORMAL, gText_DPadLeft, 0) - 4;
    s32 xRight = xChoice + choiceWidth + 4;

    FillWindowPixelRect(WIN_OPTIONS, PIXEL_FILL(1), 104, y, 96, 16);

    if (selection > 0)
        AddTextPrinterParameterized(WIN_OPTIONS, FONT_NORMAL, gText_DPadLeft, xLeft, y, TEXT_SKIP_DRAW, NULL);

    DrawOptionMenuChoice(choiceText, xChoice, y, 1, active);

    if (selection < SHINY_ODDS_OPTIONS_COUNT - 1)
        AddTextPrinterParameterized(WIN_OPTIONS, FONT_NORMAL, gText_DPadRight, xRight, y, TEXT_SKIP_DRAW, NULL);
}

static void DrawChoices_BattleScene(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_BATTLE_BATTLESCENE);
    const u8 *choiceText = sBattleSceneOptions[selection];
    s32 choiceWidth = GetStringWidth(FONT_NORMAL, choiceText, 0);
    s32 xChoice = 152 - (choiceWidth / 2);
    s32 xLeft = xChoice - GetStringWidth(FONT_NORMAL, gText_DPadLeft, 0) - 4;
    s32 xRight = xChoice + choiceWidth + 4;

    FillWindowPixelRect(WIN_OPTIONS, PIXEL_FILL(1), 104, y, 96, 16);

    if (selection > 0)
        AddTextPrinterParameterized(WIN_OPTIONS, FONT_NORMAL, gText_DPadLeft, xLeft, y, TEXT_SKIP_DRAW, NULL);

    DrawOptionMenuChoice(choiceText, xChoice, y, 1, active);

    if (selection < BATTLE_SCENE_OPTIONS_COUNT - 1)
        AddTextPrinterParameterized(WIN_OPTIONS, FONT_NORMAL, gText_DPadRight, xRight, y, TEXT_SKIP_DRAW, NULL);
}

static void DrawChoices_BattleStyle(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_BATTLE_BATTLESTYLE);
    const u8 *choiceText = sBattleStyleOptions[selection];
    s32 choiceWidth = GetStringWidth(FONT_NORMAL, choiceText, 0);
    s32 xChoice = 152 - (choiceWidth / 2);
    s32 xLeft = xChoice - GetStringWidth(FONT_NORMAL, gText_DPadLeft, 0) - 4;
    s32 xRight = xChoice + choiceWidth + 4;

    FillWindowPixelRect(WIN_OPTIONS, PIXEL_FILL(1), 104, y, 96, 16);

    if (selection > 0)
        AddTextPrinterParameterized(WIN_OPTIONS, FONT_NORMAL, gText_DPadLeft, xLeft, y, TEXT_SKIP_DRAW, NULL);

    DrawOptionMenuChoice(choiceText, xChoice, y, 1, active);

    if (selection < BATTLE_STYLE_OPTIONS_COUNT - 1)
        AddTextPrinterParameterized(WIN_OPTIONS, FONT_NORMAL, gText_DPadRight, xRight, y, TEXT_SKIP_DRAW, NULL);
}

static void DrawChoices_FrameType(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_INTERFACE_FRAMETYPE);
    const u8 *choiceText = sFrameTypeOptions[selection];
    s32 choiceWidth = GetStringWidth(FONT_NORMAL, choiceText, 0);
    s32 xChoice = 152 - (choiceWidth / 2);
    s32 xLeft = xChoice - GetStringWidth(FONT_NORMAL, gText_DPadLeft, 0) - 4;
    s32 xRight = xChoice + choiceWidth + 4;

    FillWindowPixelRect(WIN_OPTIONS, PIXEL_FILL(1), 104, y, 96, 16);

    if (selection > 0)
        AddTextPrinterParameterized(WIN_OPTIONS, FONT_NORMAL, gText_DPadLeft, xLeft, y, TEXT_SKIP_DRAW, NULL);

    DrawOptionMenuChoice(choiceText, xChoice, y, 1, active);

    if (selection < FRAME_TYPE_OPTIONS_COUNT - 1)
        AddTextPrinterParameterized(WIN_OPTIONS, FONT_NORMAL, gText_DPadRight, xRight, y, TEXT_SKIP_DRAW, NULL);
}

static void DrawChoices_ScrollBgs(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_INTERFACE_SCROLLBGS);
    const u8 *choiceText = sScrollBgsOptions[selection];
    s32 choiceWidth = GetStringWidth(FONT_NORMAL, choiceText, 0);
    s32 xChoice = 152 - (choiceWidth / 2);
    s32 xLeft = xChoice - GetStringWidth(FONT_NORMAL, gText_DPadLeft, 0) - 4;
    s32 xRight = xChoice + choiceWidth + 4;

    FillWindowPixelRect(WIN_OPTIONS, PIXEL_FILL(1), 104, y, 96, 16);

    if (selection > 0)
        AddTextPrinterParameterized(WIN_OPTIONS, FONT_NORMAL, gText_DPadLeft, xLeft, y, TEXT_SKIP_DRAW, NULL);

    DrawOptionMenuChoice(choiceText, xChoice, y, 1, active);

    if (selection < SCROLL_BGS_OPTIONS_COUNT - 1)
        AddTextPrinterParameterized(WIN_OPTIONS, FONT_NORMAL, gText_DPadRight, xRight, y, TEXT_SKIP_DRAW, NULL);
}

static void DrawChoices_ClockMode(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_INTERFACE_CLOCKMODE);
    const u8 *choiceText = sClockModeOptions[selection];
    s32 choiceWidth = GetStringWidth(FONT_NORMAL, choiceText, 0);
    s32 xChoice = 152 - (choiceWidth / 2);
    s32 xLeft = xChoice - GetStringWidth(FONT_NORMAL, gText_DPadLeft, 0) - 4;
    s32 xRight = xChoice + choiceWidth + 4;

    FillWindowPixelRect(WIN_OPTIONS, PIXEL_FILL(1), 104, y, 96, 16);

    if (selection > 0)
        AddTextPrinterParameterized(WIN_OPTIONS, FONT_NORMAL, gText_DPadLeft, xLeft, y, TEXT_SKIP_DRAW, NULL);

    DrawOptionMenuChoice(choiceText, xChoice, y, 1, active);

    if (selection < CLOCK_MODE_OPTIONS_COUNT - 1)
        AddTextPrinterParameterized(WIN_OPTIONS, FONT_NORMAL, gText_DPadRight, xRight, y, TEXT_SKIP_DRAW, NULL);
}

static void DrawChoices_IVEVDisplay(int selection, int y)
{
    bool8 active = CheckConditions(MENUITEM_INTERFACE_IVEVDISPLAY);
    const u8 *choiceText = sIVEVDisplayOptions[selection];
    s32 choiceWidth = GetStringWidth(FONT_NORMAL, choiceText, 0);
    s32 xChoice = 152 - (choiceWidth / 2);
    s32 xLeft = xChoice - GetStringWidth(FONT_NORMAL, gText_DPadLeft, 0) - 4;
    s32 xRight = xChoice + choiceWidth + 4;

    FillWindowPixelRect(WIN_OPTIONS, PIXEL_FILL(1), 104, y, 96, 16);

    if (selection > 0)
        AddTextPrinterParameterized(WIN_OPTIONS, FONT_NORMAL, gText_DPadLeft, xLeft, y, TEXT_SKIP_DRAW, NULL);

    DrawOptionMenuChoice(choiceText, xChoice, y, 1, active);

    if (selection < IV_EV_DISPLAY_OPTIONS_COUNT - 1)
        AddTextPrinterParameterized(WIN_OPTIONS, FONT_NORMAL, gText_DPadRight, xRight, y, TEXT_SKIP_DRAW, NULL);
}

// Background tilemap
#define TILE_TOP_CORNER_L 0x1A2 // 418
#define TILE_TOP_EDGE     0x1A3 // 419
#define TILE_TOP_CORNER_R 0x1A4 // 420
#define TILE_LEFT_EDGE    0x1A5 // 421
#define TILE_RIGHT_EDGE   0x1A7 // 423
#define TILE_BOT_CORNER_L 0x1A8 // 424
#define TILE_BOT_EDGE     0x1A9 // 425
#define TILE_BOT_CORNER_R 0x1AA // 426

static void DrawBgWindowFrames(void)
{
    //                     bg, tile,              x, y, width, height, palNum
    // Option Texts window
    //FillBgTilemapBufferRect(1, TILE_TOP_CORNER_L,  1,  2,  1,  1,  7);
    //FillBgTilemapBufferRect(1, TILE_TOP_EDGE,      2,  2, 26,  1,  7);
    //FillBgTilemapBufferRect(1, TILE_TOP_CORNER_R, 28,  2,  1,  1,  7);
    //FillBgTilemapBufferRect(1, TILE_LEFT_EDGE,     1,  3,  1, 16,  7);
    //FillBgTilemapBufferRect(1, TILE_RIGHT_EDGE,   28,  3,  1, 16,  7);
    //FillBgTilemapBufferRect(1, TILE_BOT_CORNER_L,  1, 13,  1,  1,  7);
    //FillBgTilemapBufferRect(1, TILE_BOT_EDGE,      2, 13, 26,  1,  7);
    //FillBgTilemapBufferRect(1, TILE_BOT_CORNER_R, 28, 13,  1,  1,  7);

    // Description window
    FillBgTilemapBufferRect(1, TILE_TOP_CORNER_L,  1, 14,  1,  1,  7);
    FillBgTilemapBufferRect(1, TILE_TOP_EDGE,      2, 14, 27,  1,  7);
    FillBgTilemapBufferRect(1, TILE_TOP_CORNER_R, 28, 14,  1,  1,  7);
    FillBgTilemapBufferRect(1, TILE_LEFT_EDGE,     1, 15,  1,  4,  7);
    FillBgTilemapBufferRect(1, TILE_RIGHT_EDGE,   28, 15,  1,  4,  7);
    FillBgTilemapBufferRect(1, TILE_BOT_CORNER_L,  1, 19,  1,  1,  7);
    FillBgTilemapBufferRect(1, TILE_BOT_EDGE,      2, 19, 27,  1,  7);
    FillBgTilemapBufferRect(1, TILE_BOT_CORNER_R, 28, 19,  1,  1,  7);

    CopyBgTilemapBufferToVram(1);
}
