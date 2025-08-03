#include "global.h"
#include "ui_start_menu.h"
#include "strings.h"
#include "bg.h"
#include "data.h"
#include "decompress.h"
#include "dexnav.h"
#include "event_data.h"
#include "field_weather.h"
#include "gpu_regs.h"
#include "graphics.h"
#include "item.h"
#include "item_menu.h"
#include "item_menu_icons.h"
#include "list_menu.h"
#include "item_icon.h"
#include "item_use.h"
#include "international_string_util.h"
#include "main.h"
#include "malloc.h"
#include "menu.h"
#include "menu_helpers.h"
#include "palette.h"
#include "party_menu.h"
#include "quests.h"
#include "scanline_effect.h"
#include "script.h"
#include "sound.h"
#include "string_util.h"
#include "strings.h"
#include "task.h"
#include "text_window.h"
#include "overworld.h"
#include "event_data.h"
#include "constants/items.h"
#include "constants/field_weather.h"
#include "constants/songs.h"
#include "constants/rgb.h"
#include "pokemon_icon.h"
#include "battle_pyramid.h"
#include "region_map.h"
#include "constants/battle_frontier.h"
#include "constants/layouts.h"
#include "rtc.h"
#include "pokedex.h"
#include "trainer_card.h"
#include "pokenav.h"
#include "option_menu.h"
#include "link.h"
#include "frontier_pass.h"
#include "start_menu.h"

//==========EWRAM==========//
static EWRAM_DATA struct StartMenuResources *sStartMenuDataPtr = NULL;
static EWRAM_DATA u8 *sBg1TilemapBuffer = NULL;
static EWRAM_DATA u8 *sBg2TilemapBuffer = NULL;
static EWRAM_DATA u8 gSelectedMenu = 0;
static EWRAM_DATA u8 gSavedSelectorY = 0;

//==========STATIC=DEFINES==========//
static void StartMenu_RunSetup(void);
static bool8 StartMenu_DoGfxSetup(void);
static bool8 StartMenu_InitBgs(void);
static void StartMenu_FadeAndBail(void);
static bool8 StartMenu_LoadGraphics(void);
static void PlaceStartMenuScrollIndicatorArrows(void);
static void RemoveStartMenuScrollIndicatorArrows(void);
static void StartMenu_GenerateVisibleMenuIndices(void);
static void StartMenu_CreateButtons(void);
static void StartMenu_UpdateVisibleButtons(void);
static void DestroyAllMenuButtons(void);
static void StartMenu_InitWindows(void);
static void Task_StartMenu_WaitFadeIn(u8 taskId);
static void Task_StartMenu_Main(u8 taskId);
static u32 GetHPEggCyclePercent(u32 partyIndex);
static void PrintMapNameAndTime(void);
static void CursorCallback(struct Sprite *sprite);

//==========CONST=DATA==========//
static const struct BgTemplate sStartMenuBgTemplates[] =
{
    {
        .bg = 0,    // windows used for text loading and hp bar blitting
        .charBaseIndex = 0,
        .mapBaseIndex = 31,
        .priority = 0
    },
    {
        .bg = 1,    // this bg loads the main tilemap with the header, footer, and menu options
        .charBaseIndex = 1,
        .mapBaseIndex = 30,
        .priority = 1
    },
    {
        .bg = 2,    // this bg loads the scrolling bg
        .charBaseIndex = 2,
        .mapBaseIndex = 28,
        .priority = 2
    }
};

static const struct WindowTemplate sStartMenuWindowTemplates[] =
{
    [WINDOW_HP_BARS] =      // Window ID the HP bars are blitted to
    {
        .bg = 0,            // which bg to print text on
        .tilemapLeft = 1,   // position from left (per 8 pixels)
        .tilemapTop = 3,    // position from top (per 8 pixels)
        .width = 9,         // width (per 8 pixels)
        .height = 15,       // height (per 8 pixels)
        .paletteNum = 1,    // palette index to use for text
        .baseBlock = 1,     // tile start in VRAM
    },

    [WINDOW_TOP_BAR] =                 // Window ID for the Time and Map name
    {
        .bg = 0,                       // which bg to print text on
        .tilemapLeft = 0,              // position from left (per 8 pixels)
        .tilemapTop = 0,               // position from top (per 8 pixels)
        .width = 30,                   // width (per 8 pixels)
        .height = 2,                   // height (per 8 pixels)
        .paletteNum = 0,               // palette index to use for text
        .baseBlock = 1 + (9 * 15),     // tile start in VRAM
    },

    [WINDOW_BOTTOM_BAR] =                         // Window ID for the save confirmation box
    {
        .bg = 0,                                  // which bg to print text on
        .tilemapLeft = 0,                         // position from left (per 8 pixels)
        .tilemapTop = 18,                         // position from top (per 8 pixels)
        .width = 30,                              // width (per 8 pixels)
        .height = 2,                              // height (per 8 pixels)
        .paletteNum = 0,                          // palette index to use for text
        .baseBlock = 1 + (9 * 15) + (30 * 2),     // tile start in VRAM
    },
    DUMMY_WIN_TEMPLATE
};

static const u32 sStartMenuTiles[] = INCBIN_U32("graphics/start_menu/menu.4bpp.lz");
static const u16 sStartMenuPalette[] = INCBIN_U16("graphics/start_menu/menu.gbapal");
static const u32 sStartMenuTilemap[] = INCBIN_U32("graphics/start_menu/menu_tilemap.bin.lz");

static const u32 sScrollBgTiles[] = INCBIN_U32("graphics/start_menu/scroll_bg/tiles.4bpp.lz");
static const u32 sScrollBgTilemap[] = INCBIN_U32("graphics/start_menu/scroll_bg/tilemap.bin.lz");

static const u16 sCursor_Pal[] = INCBIN_U16("graphics/start_menu/cursor.gbapal");
static const u32 sCursor_Gfx[] = INCBIN_U32("graphics/start_menu/cursor.4bpp.lz");

//static const u16 sIconBox_Pal[] = INCBIN_U16("graphics/start_menu/icon_box.gbapal");
//static const u32 sIconBox_Gfx[] = INCBIN_U32("graphics/start_menu/icon_box.4bpp.lz");

static const u8 sHPBar_100_Percent_Gfx[]  = INCBIN_U8("graphics/start_menu/hp_bar/100_percent.4bpp");
static const u8 sHPBar_90_Percent_Gfx[]   = INCBIN_U8("graphics/start_menu/hp_bar/90_percent.4bpp");
static const u8 sHPBar_80_Percent_Gfx[]   = INCBIN_U8("graphics/start_menu/hp_bar/80_percent.4bpp");
static const u8 sHPBar_70_Percent_Gfx[]   = INCBIN_U8("graphics/start_menu/hp_bar/70_percent.4bpp");
static const u8 sHPBar_60_Percent_Gfx[]   = INCBIN_U8("graphics/start_menu/hp_bar/60_percent.4bpp");
static const u8 sHPBar_50_Percent_Gfx[]   = INCBIN_U8("graphics/start_menu/hp_bar/50_percent.4bpp");
static const u8 sHPBar_40_Percent_Gfx[]   = INCBIN_U8("graphics/start_menu/hp_bar/40_percent.4bpp");
static const u8 sHPBar_30_Percent_Gfx[]   = INCBIN_U8("graphics/start_menu/hp_bar/30_percent.4bpp");
static const u8 sHPBar_20_Percent_Gfx[]   = INCBIN_U8("graphics/start_menu/hp_bar/20_percent.4bpp");
static const u8 sHPBar_10_Percent_Gfx[]   = INCBIN_U8("graphics/start_menu/hp_bar/10_percent.4bpp");
static const u8 sHPBar_0_Percent_Gfx[]    = INCBIN_U8("graphics/start_menu/hp_bar/0_percent.4bpp");
static const u16 sHPBar_Pal[] = INCBIN_U16("graphics/start_menu/hp_bar/hpbar.gbapal");

static const u8 sMenuButtonA_Gfx[] = INCBIN_U8("graphics/start_menu/menu_sprites/menu_buttons_a.4bpp");
static const u16 sMenuButtonA_Pal[] = INCBIN_U16("graphics/start_menu/menu_sprites/menu_buttons_a.gbapal");
static const u8 sMenuButtonB_Gfx[] = INCBIN_U8("graphics/start_menu/menu_sprites/menu_buttons_b.4bpp");
static const u16 sMenuButtonB_Pal[] = INCBIN_U16("graphics/start_menu/menu_sprites/menu_buttons_b.gbapal");

#define TAG_CURSOR         30004
//#define TAG_ICON_BOX       30006
#define TAG_HPBAR          30008
#define TAG_STATUS_ICON    30009
#define TAG_BUTTON_ICON_A  30020
#define TAG_BUTTON_ICON_B  30021

// Cursor
static const struct OamData sOamData_Cursor =
{
    .size = SPRITE_SIZE(64x32),
    .shape = SPRITE_SHAPE(64x32),
    .priority = 1,
};

static const struct CompressedSpriteSheet sSpriteSheet_Cursor =
{
    .data = sCursor_Gfx,
    .size = 64*32*4/2,
    .tag = TAG_CURSOR,
};

static const struct SpritePalette sSpritePal_Cursor =
{
    .data = sCursor_Pal,
    .tag = TAG_CURSOR
};

static const union AnimCmd sSpriteAnim_Cursor1[] =
{
    ANIMCMD_FRAME(0, 32),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd sSpriteAnim_Cursor2[] =
{
    ANIMCMD_FRAME(32, 32),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd *const sSpriteAnimTable_Cursor[] =
{
    sSpriteAnim_Cursor1,
    sSpriteAnim_Cursor2,
};

static const struct SpriteTemplate sSpriteTemplate_Cursor =
{
    .tileTag = TAG_CURSOR,
    .paletteTag = TAG_CURSOR,
    .oam = &sOamData_Cursor,
    .anims = sSpriteAnimTable_Cursor,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = CursorCallback
};

/*
// Icon Box
static const struct OamData sOamData_IconBox =
{
    .size = SPRITE_SIZE(32x32),
    .shape = SPRITE_SHAPE(32x32),
    .priority = 2,
};

static const struct CompressedSpriteSheet sSpriteSheet_IconBox =
{
    .data = sIconBox_Gfx,
    .size = 32*32*4/2,
    .tag = TAG_ICON_BOX,
};

static const struct SpritePalette sSpritePal_IconBox =
{
    .data = sIconBox_Pal,
    .tag = TAG_ICON_BOX
};

static const union AnimCmd sSpriteAnim_IconBox[] =
{
    ANIMCMD_FRAME(0, 32),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd *const sSpriteAnimTable_IconBox[] =
{
    sSpriteAnim_IconBox,
};

static const struct SpriteTemplate sSpriteTemplate_IconBox =
{
    .tileTag = TAG_ICON_BOX,
    .paletteTag = TAG_ICON_BOX,
    .oam = &sOamData_IconBox,
    .anims = sSpriteAnimTable_IconBox,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy
};
*/

// Status Icons
static const struct OamData sOamData_StatusCondition =
{
    .y = 0,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .mosaic = FALSE,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(32x8),
    .x = 0,
    .matrixNum = 0,
    .size = SPRITE_SIZE(32x8),
    .tileNum = 0,
    .priority = 1,
    .paletteNum = 0,
    .affineParam = 0
};

static const union AnimCmd sSpriteAnim_StatusPoison[] =
{
    ANIMCMD_FRAME(0, 0),
    ANIMCMD_END
};

static const union AnimCmd sSpriteAnim_StatusParalyzed[] =
{
    ANIMCMD_FRAME(4, 0),
    ANIMCMD_END
};

static const union AnimCmd sSpriteAnim_StatusSleep[] =
{
    ANIMCMD_FRAME(8, 0),
    ANIMCMD_END
};

static const union AnimCmd sSpriteAnim_StatusFrozen[] =
{
    ANIMCMD_FRAME(12, 0),
    ANIMCMD_END
};

static const union AnimCmd sSpriteAnim_StatusBurn[] =
{
    ANIMCMD_FRAME(16, 0),
    ANIMCMD_END
};

static const union AnimCmd sSpriteAnim_StatusPokerus[] =
{
    ANIMCMD_FRAME(20, 0),
    ANIMCMD_END
};

static const union AnimCmd sSpriteAnim_StatusFaint[] =
{
    ANIMCMD_FRAME(24, 0),
    ANIMCMD_END
};

static const union AnimCmd sSpriteAnim_StatusFrostbite[] =
{
    ANIMCMD_FRAME(28, 0),
    ANIMCMD_END
};

static const union AnimCmd sSpriteAnim_Blank[] =
{
    ANIMCMD_FRAME(32, 0),
    ANIMCMD_END
};

static const union AnimCmd *const sSpriteTemplate_StatusCondition[] =
{
    sSpriteAnim_StatusPoison,
    sSpriteAnim_StatusParalyzed,
    sSpriteAnim_StatusSleep,
    sSpriteAnim_StatusFrozen,
    sSpriteAnim_StatusBurn,
    sSpriteAnim_StatusPokerus,
    sSpriteAnim_StatusFaint,
    sSpriteAnim_StatusFrostbite,
    sSpriteAnim_Blank
};

static const struct CompressedSpriteSheet sSpriteSheet_StatusIcons =
{
    gStatusGfx_Icons, 0x400, TAG_STATUS_ICON
};

static const struct SpritePalette sSpritePalette_StatusIcons =
{
    gStatusPal_Icons, TAG_STATUS_ICON
};

static const struct SpriteTemplate sSpriteTemplate_StatusIcons =
{
    .tileTag = TAG_STATUS_ICON,
    .paletteTag = TAG_STATUS_ICON,
    .oam = &sOamData_StatusCondition,
    .anims = sSpriteTemplate_StatusCondition,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

// Menu Button A
static const struct SpriteSheet sSpriteSheet_MenuButtonA =
{
    .data = sMenuButtonA_Gfx,
    .size = 64 * 32 * START_MENU_BUTTON_A_COUNT / 2,
    .tag = TAG_BUTTON_ICON_A,
};

static const struct SpritePalette sSpritePalette_MenuButtonA =
{
    .data = sMenuButtonA_Pal,
    .tag = TAG_BUTTON_ICON_A,
};

static const struct OamData sOam_MenuButtonA =
{
    .shape = SPRITE_SHAPE(64x32),
    .size = SPRITE_SIZE(64x32),
    .priority = 2,
    .paletteNum = 5,
};

static const union AnimCmd sAnim_MenuButtonA_Pokedex1[] =
{
    ANIMCMD_FRAME(0, 0),
    ANIMCMD_END
};

static const union AnimCmd sAnim_MenuButtonA_Pokedex2[] =
{
    ANIMCMD_FRAME(32, 0),
    ANIMCMD_END
};

static const union AnimCmd sAnim_MenuButtonA_Pokemon1[] =
{
    ANIMCMD_FRAME(64, 0),
    ANIMCMD_END
};

static const union AnimCmd sAnim_MenuButtonA_Pokemon2[] =
{
    ANIMCMD_FRAME(96, 0),
    ANIMCMD_END
};

static const union AnimCmd sAnim_MenuButtonA_Bag1[] =
{
    ANIMCMD_FRAME(128, 0),
    ANIMCMD_END
};

static const union AnimCmd sAnim_MenuButtonA_Bag2[] =
{
    ANIMCMD_FRAME(160, 0),
    ANIMCMD_END
};

static const union AnimCmd sAnim_MenuButtonA_Card1[] =
{
    ANIMCMD_FRAME(192, 0),
    ANIMCMD_END
};

static const union AnimCmd sAnim_MenuButtonA_Card2[] =
{
    ANIMCMD_FRAME(224, 0),
    ANIMCMD_END
};

static const union AnimCmd sAnim_MenuButtonA_Quests1[] =
{
    ANIMCMD_FRAME(256, 0),
    ANIMCMD_END
};

static const union AnimCmd sAnim_MenuButtonA_Quests2[] =
{
    ANIMCMD_FRAME(288, 0),
    ANIMCMD_END
};

static const union AnimCmd *const sAnimTable_MenuButtonA[] =
{
    sAnim_MenuButtonA_Pokedex1,
    sAnim_MenuButtonA_Pokedex2,
    sAnim_MenuButtonA_Pokemon1,
    sAnim_MenuButtonA_Pokemon2,
    sAnim_MenuButtonA_Bag1,
    sAnim_MenuButtonA_Bag2,
    sAnim_MenuButtonA_Card1,
    sAnim_MenuButtonA_Card2,
    sAnim_MenuButtonA_Quests1,
    sAnim_MenuButtonA_Quests2,
};

static const struct SpriteTemplate sSpriteTemplate_MenuButtonA =
{
    .tileTag = TAG_BUTTON_ICON_A,
    .paletteTag = TAG_BUTTON_ICON_A,
    .oam = &sOam_MenuButtonA,
    .anims = sAnimTable_MenuButtonA,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const u8 sButtonAnimIndicesA[START_MENU_BUTTON_A_COUNT / 2][2] =
{
    {START_MENU_BUTTON_POKEDEX1,  START_MENU_BUTTON_POKEDEX2},
    {START_MENU_BUTTON_POKEMON1,  START_MENU_BUTTON_POKEMON2},
    {START_MENU_BUTTON_BAG1,      START_MENU_BUTTON_BAG2},
    {START_MENU_BUTTON_CARD1,     START_MENU_BUTTON_CARD2},
    {START_MENU_BUTTON_QUESTS1,   START_MENU_BUTTON_QUESTS2},
};

// Menu Button B
static const struct SpriteSheet sSpriteSheet_MenuButtonB =
{
    .data = sMenuButtonB_Gfx,
    .size = 64 * 32 * START_MENU_BUTTON_B_COUNT / 2,
    .tag = TAG_BUTTON_ICON_B,
};

static const struct SpritePalette sSpritePalette_MenuButtonB =
{
    .data = sMenuButtonB_Pal,
    .tag = TAG_BUTTON_ICON_B,
};

static const struct OamData sOam_MenuButtonB =
{
    .shape = SPRITE_SHAPE(64x32),
    .size = SPRITE_SIZE(64x32),
    .priority = 2,
    .paletteNum = 6,
};

static const union AnimCmd sAnim_MenuButtonB_Options1[] =
{
    ANIMCMD_FRAME(0, 0),
    ANIMCMD_END
};

static const union AnimCmd sAnim_MenuButtonB_Options2[] =
{
    ANIMCMD_FRAME(32, 0),
    ANIMCMD_END
};

static const union AnimCmd sAnim_MenuButtonB_DexNav1[] =
{
    ANIMCMD_FRAME(64, 0),
    ANIMCMD_END
};

static const union AnimCmd sAnim_MenuButtonB_DexNav2[] =
{
    ANIMCMD_FRAME(96, 0),
    ANIMCMD_END
};

static const union AnimCmd *const sAnimTable_MenuButtonB[] =
{
    sAnim_MenuButtonB_Options1,
    sAnim_MenuButtonB_Options2,
    sAnim_MenuButtonB_DexNav1,
    sAnim_MenuButtonB_DexNav2,
};

static const struct SpriteTemplate sSpriteTemplate_MenuButtonB =
{
    .tileTag = TAG_BUTTON_ICON_B,
    .paletteTag = TAG_BUTTON_ICON_B,
    .oam = &sOam_MenuButtonB,
    .anims = sAnimTable_MenuButtonB,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const u8 sButtonAnimIndicesB[START_MENU_BUTTON_B_COUNT / 2][2] =
{
    {START_MENU_BUTTON_OPTIONS1,  START_MENU_BUTTON_OPTIONS2},
    {START_MENU_BUTTON_DEXNAV1,   START_MENU_BUTTON_DEXNAV2},
};

#define CURSOR_LEFT_COL_X    START_MENU_BUTTON_X_1
#define CURSOR_RIGHT_COL_X   START_MENU_BUTTON_X_2
#define CURSOR_TOP_ROW_Y     START_MENU_BUTTON_Y_START
#define CURSOR_MID1_ROW_Y    START_MENU_BUTTON_Y_START + (START_MENU_BUTTON_Y_SPACING * 1)
#define CURSOR_MID2_ROW_Y    START_MENU_BUTTON_Y_START + (START_MENU_BUTTON_Y_SPACING * 2)
#define CURSOR_BTM_ROW_Y     START_MENU_BUTTON_Y_START + (START_MENU_BUTTON_Y_SPACING * 3)

static void CreateCursor(void)
{
    if (sStartMenuDataPtr->cursorSpriteIds[0] == SPRITE_NONE && sStartMenuDataPtr->cursorSpriteIds[1] == SPRITE_NONE)
    {
        sStartMenuDataPtr->cursorSpriteIds[0] = CreateSprite(&sSpriteTemplate_Cursor, CURSOR_LEFT_COL_X, CURSOR_TOP_ROW_Y, 0);
        sStartMenuDataPtr->cursorSpriteIds[1] = CreateSprite(&sSpriteTemplate_Cursor, CURSOR_LEFT_COL_X + 64, CURSOR_TOP_ROW_Y, 0);

        StartSpriteAnim(&gSprites[sStartMenuDataPtr->cursorSpriteIds[0]], 0);
        StartSpriteAnim(&gSprites[sStartMenuDataPtr->cursorSpriteIds[1]], 1);

        gSprites[sStartMenuDataPtr->cursorSpriteIds[0]].invisible = FALSE;
        gSprites[sStartMenuDataPtr->cursorSpriteIds[1]].invisible = FALSE;
    }
}

static void DestroyCursor(void)
{
    for (int i = 0; i < 2; i++)
    {
        if (sStartMenuDataPtr->cursorSpriteIds[i] != SPRITE_NONE)
        {
            DestroySprite(&gSprites[sStartMenuDataPtr->cursorSpriteIds[i]]);
            sStartMenuDataPtr->cursorSpriteIds[i] = SPRITE_NONE;
        }
    }
}

struct SpriteCordsStruct {
    u8 x;
    u8 y;
};

static void CursorCallback(struct Sprite *sprite)
{
    static const u8 sCursorYCoords[4] = {
        CURSOR_TOP_ROW_Y,
        CURSOR_MID1_ROW_Y,
        CURSOR_MID2_ROW_Y,
        CURSOR_BTM_ROW_Y,
    };

    u8 y = sStartMenuDataPtr->selector_y;

    if (sprite == &gSprites[sStartMenuDataPtr->cursorSpriteIds[0]])
    {
        sprite->x = CURSOR_LEFT_COL_X;
        sprite->y = sCursorYCoords[y];
    }
    else if (sprite == &gSprites[sStartMenuDataPtr->cursorSpriteIds[1]])
    {
        sprite->x = CURSOR_LEFT_COL_X + 64;
        sprite->y = sCursorYCoords[y];
    }

    for (int i = 0; i < VISIBLE_BUTTONS * 2; i += 2)
    {
        u8 buttonSpriteId = sStartMenuDataPtr->MenuButtonSpriteIds[i];
        struct Sprite *buttonSprite = &gSprites[buttonSpriteId];

        if (abs(sprite->y - buttonSprite->y) < 4)
        {
            gSelectedMenu = sStartMenuDataPtr->scrollOffset + (i / 2);
            break;
        }
    }
}

static void InitCursorInPlace(void)
{
    if (gSelectedMenu >= sStartMenuDataPtr->numVisibleMenuItems)
        gSelectedMenu = sStartMenuDataPtr->numVisibleMenuItems - 1;

    u8 maxVisible = sStartMenuDataPtr->numVisibleMenuItems;
    if (maxVisible > VISIBLE_BUTTONS)
        maxVisible = VISIBLE_BUTTONS;

    if (gSavedSelectorY >= maxVisible)
        gSavedSelectorY = maxVisible - 1;

    int scrollOffsetCandidate = gSelectedMenu - gSavedSelectorY;
    if (scrollOffsetCandidate < 0)
        scrollOffsetCandidate = 0;
    if (scrollOffsetCandidate > (sStartMenuDataPtr->numVisibleMenuItems - maxVisible))
        scrollOffsetCandidate = sStartMenuDataPtr->numVisibleMenuItems - maxVisible;

    if (scrollOffsetCandidate < 0)
        scrollOffsetCandidate = 0;

    sStartMenuDataPtr->scrollOffset = scrollOffsetCandidate;
    sStartMenuDataPtr->selector_y = gSelectedMenu - scrollOffsetCandidate;
}

#define ICON_BOX_1_START_X          24
#define ICON_BOX_1_START_Y          40
#define ICON_BOX_X_DIFFERENCE       40
#define ICON_BOX_Y_DIFFERENCE       40

/*
static void CreateIconBox(void)
{
    u8 i = 0;

    sStartMenuDataPtr->iconBoxSpriteIds[0] = CreateSprite(&sSpriteTemplate_IconBox, ICON_BOX_1_START_X, ICON_BOX_1_START_Y, 2);
    sStartMenuDataPtr->iconBoxSpriteIds[1] = CreateSprite(&sSpriteTemplate_IconBox, ICON_BOX_1_START_X + ICON_BOX_X_DIFFERENCE, ICON_BOX_1_START_Y, 2);

    sStartMenuDataPtr->iconBoxSpriteIds[2] = CreateSprite(&sSpriteTemplate_IconBox, ICON_BOX_1_START_X, ICON_BOX_1_START_Y + (ICON_BOX_X_DIFFERENCE * 1), 2);
    sStartMenuDataPtr->iconBoxSpriteIds[3] = CreateSprite(&sSpriteTemplate_IconBox, ICON_BOX_1_START_X + ICON_BOX_X_DIFFERENCE, ICON_BOX_1_START_Y + (ICON_BOX_Y_DIFFERENCE * 1), 2);

    sStartMenuDataPtr->iconBoxSpriteIds[4] = CreateSprite(&sSpriteTemplate_IconBox, ICON_BOX_1_START_X, ICON_BOX_1_START_Y + (ICON_BOX_X_DIFFERENCE * 2), 2);
    sStartMenuDataPtr->iconBoxSpriteIds[5] = CreateSprite(&sSpriteTemplate_IconBox, ICON_BOX_1_START_X + ICON_BOX_X_DIFFERENCE, ICON_BOX_1_START_Y + (ICON_BOX_Y_DIFFERENCE * 2), 2);

    for(i = 0; i < 6; i++)
    {
        gSprites[sStartMenuDataPtr->iconBoxSpriteIds[i]].invisible = FALSE;
        gSprites[sStartMenuDataPtr->iconBoxSpriteIds[i]].oam.objMode = ST_OAM_OBJ_BLEND;
        StartSpriteAnim(&gSprites[sStartMenuDataPtr->iconBoxSpriteIds[i]], 0);
        gSprites[sStartMenuDataPtr->iconBoxSpriteIds[i]].oam.priority = 2;
    }

    SetGpuReg(REG_OFFSET_BLDCNT, BLDCNT_TGT2_ALL);
    SetGpuReg(REG_OFFSET_BLDALPHA, BLDALPHA_BLEND(7, 11));

    return;
}

static void DestroyIconBoxes(void)
{
    u8 i = 0;
    for(i = 0; i < 6; i++)
    {
        DestroySprite(&gSprites[sStartMenuDataPtr->iconBoxSpriteIds[i]]);
        sStartMenuDataPtr->iconBoxSpriteIds[i] = SPRITE_NONE;
    }
}
*/

static void CreatePartyMonIcons(void)
{
    u8 i = 0;
    s16 x = ICON_BOX_1_START_X;
    s16 y = ICON_BOX_1_START_Y;
    LoadMonIconPalettes();
    for(i = 0; i < gPlayerPartyCount; i++)
    {   
        switch (i) // choose position for each icon
        {
            case 0:
                x = ICON_BOX_1_START_X;
                y = ICON_BOX_1_START_Y;
                break;
            case 1:
                x = ICON_BOX_1_START_X + ICON_BOX_X_DIFFERENCE;
                y = ICON_BOX_1_START_Y;
                break;
            case 2:
                x = ICON_BOX_1_START_X;
                y = ICON_BOX_1_START_Y + (ICON_BOX_Y_DIFFERENCE * 1);
                break;
            case 3:
                x = ICON_BOX_1_START_X + ICON_BOX_X_DIFFERENCE;
                y = ICON_BOX_1_START_Y + (ICON_BOX_Y_DIFFERENCE * 1);
                break;
            case 4:
                x = ICON_BOX_1_START_X;
                y = ICON_BOX_1_START_Y + (ICON_BOX_X_DIFFERENCE * 2);
                break;
            case 5:
                x = ICON_BOX_1_START_X + ICON_BOX_X_DIFFERENCE;
                y = ICON_BOX_1_START_Y + (ICON_BOX_Y_DIFFERENCE * 2);
                break;
        }

        sStartMenuDataPtr->iconMonSpriteIds[i] = CreateMonIcon(GetMonData(&gPlayerParty[i], MON_DATA_SPECIES_OR_EGG), SpriteCB_MonIcon, x, y, 0, GetMonData(&gPlayerParty[i], MON_DATA_PERSONALITY));

        gSprites[sStartMenuDataPtr->iconMonSpriteIds[i]].oam.priority = 0;

        if (GetHPEggCyclePercent(i) == 0)
        {
            gSprites[sStartMenuDataPtr->iconMonSpriteIds[i]].callback = SpriteCallbackDummy;
        }

    }
}

static void DestroyMonIcons(void)
{
    u8 i = 0;
    for(i = 0; i < 6; i++)
    {
        DestroySprite(&gSprites[sStartMenuDataPtr->iconMonSpriteIds[i]]);
        sStartMenuDataPtr->iconMonSpriteIds[i] = SPRITE_NONE;
    }
}

static const u8 *GetBarGfx(u32 percent)
{
    u32 modifiedPercent = percent / 10;

    const u8 *sHPBar_Fill_Gfx[] = {
        sHPBar_0_Percent_Gfx,
        sHPBar_10_Percent_Gfx,
        sHPBar_20_Percent_Gfx,
        sHPBar_30_Percent_Gfx,
        sHPBar_40_Percent_Gfx,
        sHPBar_50_Percent_Gfx,
        sHPBar_60_Percent_Gfx,
        sHPBar_70_Percent_Gfx,
        sHPBar_80_Percent_Gfx,
        sHPBar_90_Percent_Gfx,
        sHPBar_100_Percent_Gfx,
    };

    if (percent == 0)
        return sHPBar_Fill_Gfx[percent];

    if (modifiedPercent == 0)
        return sHPBar_Fill_Gfx[1];

    return sHPBar_Fill_Gfx[modifiedPercent];
}

static bool32 IsMonNotEmpty(u32 partyIndex)
{
    return (GetMonData(&gPlayerParty[partyIndex], MON_DATA_SPECIES_OR_EGG, NULL) != SPECIES_NONE);
}

static u32 GetHPEggCyclePercent(u32 partyIndex)
{
    struct Pokemon *mon = &gPlayerParty[partyIndex];
    if (!GetMonData(mon, MON_DATA_IS_EGG))
        return ((GetMonData(mon, MON_DATA_HP)) * 100 / (GetMonData(mon,MON_DATA_MAX_HP)));
    else
        return ((GetMonData(mon, MON_DATA_FRIENDSHIP)) * 100 / (gSpeciesInfo[GetMonData(mon,MON_DATA_SPECIES)].eggCycles));
}

#define HP_BAR_X_START  0
#define HP_BAR_Y_START  30

static void StartMenu_DisplayHP(void)
{
    u32 i;
    u32 y = 1;
    s32 x = -4;

    FillWindowPixelBuffer(WINDOW_HP_BARS, PIXEL_FILL(TEXT_COLOR_TRANSPARENT));

    for(i = 0; i < PARTY_SIZE; i++) // choose position for each hp bar
    {
        switch (i)
        {   
            case 0:
                x = HP_BAR_X_START;
                y = HP_BAR_Y_START;
                break;
            case 1:
                x = HP_BAR_X_START + ICON_BOX_X_DIFFERENCE;
                y = HP_BAR_Y_START;
                break;
            case 2:
                x = HP_BAR_X_START;
                y = HP_BAR_Y_START + (ICON_BOX_Y_DIFFERENCE * 1);
                break;
            case 3:
                x = HP_BAR_X_START + ICON_BOX_X_DIFFERENCE;
                y = HP_BAR_Y_START + (ICON_BOX_Y_DIFFERENCE * 1);
                break;
            case 4:
                x = HP_BAR_X_START;
                y = HP_BAR_Y_START + (ICON_BOX_X_DIFFERENCE * 2);
                break;
            case 5:
                x = HP_BAR_X_START + ICON_BOX_X_DIFFERENCE;
                y = HP_BAR_Y_START + (ICON_BOX_Y_DIFFERENCE * 2);
                break;
        }

        if(!IsMonNotEmpty(i))
            continue;

        BlitBitmapToWindow(WINDOW_HP_BARS, GetBarGfx(GetHPEggCyclePercent(i)), x, y, 32, 8);
    }

    CopyWindowToVram(WINDOW_HP_BARS, COPYWIN_GFX);
}

#define AILMENT_NONE  0
#define AILMENT_PSN   1
#define AILMENT_PRZ   2
#define AILMENT_SLP   3
#define AILMENT_FRZ   4
#define AILMENT_BRN   5
#define AILMENT_PKRS  6
#define AILMENT_FNT   7
#define AILMENT_FRB   8

#define ICON_STATUS_1_START_X  24
#define ICON_STATUS_1_START_Y  29

static void CreatePartyMonStatuses(void)
{
    u8 i = 0;
    s16 x = ICON_STATUS_1_START_X;
    s16 y = ICON_STATUS_1_START_Y;
    u8 status;

    for(i = 0; i < gPlayerPartyCount; i++)
    {   
        switch (i)
        {
            case 0:
                x = ICON_STATUS_1_START_X;
                y = ICON_STATUS_1_START_Y;
                break;
            case 1:
                x = ICON_STATUS_1_START_X + ICON_BOX_X_DIFFERENCE;
                y = ICON_STATUS_1_START_Y;
                break;
            case 2:
                x = ICON_STATUS_1_START_X;
                y = ICON_STATUS_1_START_Y + (ICON_BOX_Y_DIFFERENCE * 1);
                break;
            case 3:
                x = ICON_STATUS_1_START_X + ICON_BOX_X_DIFFERENCE;
                y = ICON_STATUS_1_START_Y + (ICON_BOX_Y_DIFFERENCE * 1);
                break;
            case 4:
                x = ICON_STATUS_1_START_X;
                y = ICON_STATUS_1_START_Y + (ICON_BOX_X_DIFFERENCE * 2);
                break;
            case 5:
                x = ICON_STATUS_1_START_X + ICON_BOX_X_DIFFERENCE;
                y = ICON_STATUS_1_START_Y + (ICON_BOX_Y_DIFFERENCE * 2);
                break;
        }

        sStartMenuDataPtr->iconStatusSpriteIds[i] = CreateSprite(&sSpriteTemplate_StatusIcons, x, y, 0);

        status = GetMonAilment(&gPlayerParty[i]);
        switch (status)
        {
            case AILMENT_NONE:
            case AILMENT_PKRS:
                gSprites[sStartMenuDataPtr->iconStatusSpriteIds[i]].invisible = TRUE;
                break;
            default:
                StartSpriteAnim(&gSprites[sStartMenuDataPtr->iconStatusSpriteIds[i]], status - 1);
                gSprites[sStartMenuDataPtr->iconStatusSpriteIds[i]].invisible = FALSE;
                break;
        }
        gSprites[sStartMenuDataPtr->iconStatusSpriteIds[i]].oam.priority = 0;
    }
}

static void DestroyStatusSprites(void)
{
    u8 i = 0;
    for(i = 0; i < 6; i++)
    {
        DestroySprite(&gSprites[sStartMenuDataPtr->iconStatusSpriteIds[i]]);
        sStartMenuDataPtr->iconStatusSpriteIds[i] = SPRITE_NONE;
    }
}


//==========FUNCTIONS==========//
void Task_StartMenu_Open(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        CleanupOverworldWindowsAndTilemaps();
        StartMenu_Init(CB2_ReturnToField);
        DestroyTask(taskId);
    }
}

void StartMenu_Init(MainCallback callback)
{
    u32 i = 0;
    if ((sStartMenuDataPtr = AllocZeroed(sizeof(struct StartMenuResources))) == NULL)
    {
        SetMainCallback2(callback);
        return;
    }
    
    // Make Sure Sprites are Empty on Reload
    sStartMenuDataPtr->gfxLoadState = 0;
    sStartMenuDataPtr->savedCallback = callback;
    sStartMenuDataPtr->cursorSpriteIds[0] = SPRITE_NONE;
    sStartMenuDataPtr->cursorSpriteIds[1] = SPRITE_NONE;
    sStartMenuDataPtr->scrollIndicatorArrowPairId = SPRITE_NONE;

    StartMenu_GenerateVisibleMenuIndices();

    for(i= 0; i < 6; i++)
    {
        //sStartMenuDataPtr->iconBoxSpriteIds[i] = SPRITE_NONE;
        sStartMenuDataPtr->iconMonSpriteIds[i] = SPRITE_NONE;
    }

    InitCursorInPlace();

    gFieldCallback = NULL;
    
    SetMainCallback2(StartMenu_RunSetup);
}

static void StartMenu_RunSetup(void)
{
    while (1)
    {
        if (StartMenu_DoGfxSetup() == TRUE)
            break;
    }
}

static void StartMenu_MainCB(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    DoScheduledBgTilemapCopiesToVram();
    UpdatePaletteFade();
}

static void StartMenu_VBlankCB(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();

    if (gSaveBlock4Ptr->optionsScrollBgs == OPTIONS_SCROLL_BGS_ON)
    {
        ChangeBgX(2, 64, BG_COORD_ADD);
        ChangeBgY(2, 64, BG_COORD_ADD);
    }
}

static bool8 StartMenu_DoGfxSetup(void)
{
    switch (gMain.state)
    {
    case 0:
        DmaClearLarge16(3, (void *)VRAM, VRAM_SIZE, 0x1000)
        SetVBlankHBlankCallbacksToNull();
        ClearScheduledBgCopiesToVram();
        ResetVramOamAndBgCntRegs();
        gMain.state++;
        break;
    case 1:
        ScanlineEffect_Stop();
        FreeAllSpritePalettes();
        ResetPaletteFade();
        ResetSpriteData();
        ResetTasks();
        gMain.state++;
        break;
    case 2:
        if (StartMenu_InitBgs())
        {
            sStartMenuDataPtr->gfxLoadState = 0;
            gMain.state++;
        }
        else
        {
            StartMenu_FadeAndBail();
            return TRUE;
        }
        break;
    case 3:
        if (StartMenu_LoadGraphics() == TRUE)
            gMain.state++;
        break;
    case 4:
        StartMenu_InitWindows();
        gMain.state++;
        break;
    case 5:
        PrintMapNameAndTime(); // print all sprites
        //CreateIconBox();
        CreatePartyMonIcons();
        StartMenu_DisplayHP();
        CreatePartyMonStatuses();
        gMain.state++;
        break;
    case 6:
        CreateTask(Task_StartMenu_WaitFadeIn, 0);
        BlendPalettes(PALETTES_ALL, 16, RGB_BLACK);
        gMain.state++;
        break;
    case 7:
        BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
        gMain.state++;
        break;
    default:
        SetVBlankCallback(StartMenu_VBlankCB);
        SetMainCallback2(StartMenu_MainCB);
        return TRUE;
    }
    return FALSE;
}

#define TryFree(ptr) ({        \
    void ** ptr__ = (void **)&(ptr);   \
    if (*ptr__ != NULL)                \
        Free(*ptr__);                  \
})

static void StartMenu_FreeResources(void)
{
    TryFree(sStartMenuDataPtr);
    TryFree(sBg1TilemapBuffer);
    TryFree(sBg2TilemapBuffer);
    DestroyCursor();
    RemoveStartMenuScrollIndicatorArrows();
    DestroyAllMenuButtons();
    //DestroyIconBoxes();
    DestroyMonIcons();
    DestroyStatusSprites();
    FreeAllWindowBuffers();    
}

#undef TryFree

static void Task_StartMenu_WaitFadeAndBail(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        SetMainCallback2(sStartMenuDataPtr->savedCallback);
        StartMenu_FreeResources();
        DestroyTask(taskId);
    }
}

static void StartMenu_FadeAndBail(void)
{
    BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
    CreateTask(Task_StartMenu_WaitFadeAndBail, 0);
    SetVBlankCallback(StartMenu_VBlankCB);
    SetMainCallback2(StartMenu_MainCB);
}

static void Task_StartMenu_WaitFadeIn(u8 taskId)
{
    if (!gPaletteFade.active)
        gTasks[taskId].func = Task_StartMenu_Main;
}

static void Task_StartMenu_TurnOff(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        SetMainCallback2(sStartMenuDataPtr->savedCallback);
        StartMenu_FreeResources();
        DestroyTask(taskId);
    }
}


static bool8 StartMenu_InitBgs(void) // This function sets the bg tilemap buffers for each bg and initializes them, shows them, and turns sprites on
{
    ResetAllBgsCoordinates();
    sBg1TilemapBuffer = Alloc(0x800);
    if (sBg1TilemapBuffer == NULL)
        return FALSE;
    memset(sBg1TilemapBuffer, 0, 0x800);

    sBg2TilemapBuffer = Alloc(0x800);
    if (sBg2TilemapBuffer == NULL)
        return FALSE;
    memset(sBg2TilemapBuffer, 0, 0x800);
    
    ResetBgsAndClearDma3BusyFlags(0);
    InitBgsFromTemplates(0, sStartMenuBgTemplates, NELEMS(sStartMenuBgTemplates));
    SetBgTilemapBuffer(1, sBg1TilemapBuffer);
    SetBgTilemapBuffer(2, sBg2TilemapBuffer);
    ScheduleBgCopyTilemapToVram(1);
    ScheduleBgCopyTilemapToVram(2);
    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP);
    ShowBg(0);
    ShowBg(1);
    ShowBg(2);
    return TRUE;
}

static bool8 StartMenu_LoadGraphics(void) // Load the Tilesets, Tilemaps, Spritesheets, and Palettes for Everything
{
    switch (sStartMenuDataPtr->gfxLoadState)
    {
    case 0:
        ResetTempTileDataBuffers();
        DecompressAndCopyTileDataToVram(1, sStartMenuTiles, 0, 0, 0);
        DecompressAndCopyTileDataToVram(2, sScrollBgTiles, 0, 0, 0);
        sStartMenuDataPtr->gfxLoadState++;
        break;
    case 1:
        if (FreeTempTileDataBuffersIfPossible() != TRUE)
        {
            DecompressDataWithHeaderWram(sStartMenuTilemap, sBg1TilemapBuffer);
            DecompressDataWithHeaderWram(sScrollBgTilemap, sBg2TilemapBuffer);
            sStartMenuDataPtr->gfxLoadState++;
        }
        break;
    case 2:
    {
        struct SpritePalette cursorPal = {sSpritePal_Cursor.data, sSpritePal_Cursor.tag};
        LoadPalette(sStartMenuPalette, PLTT_ID(0), PLTT_SIZE_4BPP);
        LoadPalette(sHPBar_Pal, PLTT_ID(1), PLTT_SIZE_4BPP);

        //LoadCompressedSpriteSheet(&sSpriteSheet_IconBox);
        //LoadSpritePalette(&sSpritePal_IconBox);
        LoadCompressedSpriteSheet(&sSpriteSheet_Cursor);
        LoadSpritePalette(&cursorPal);
        LoadCompressedSpriteSheet(&sSpriteSheet_StatusIcons);
        LoadSpritePalette(&sSpritePalette_StatusIcons);
        LoadSpriteSheet(&sSpriteSheet_MenuButtonA);
        LoadSpritePalette(&sSpritePalette_MenuButtonA);
        LoadSpriteSheet(&sSpriteSheet_MenuButtonB);
        LoadSpritePalette(&sSpritePalette_MenuButtonB);

        StartMenu_CreateButtons();
        StartMenu_UpdateVisibleButtons();  
        InitCursorInPlace();
        CreateCursor();

        PlaceStartMenuScrollIndicatorArrows();

        sStartMenuDataPtr->gfxLoadState++;
        break;
    }
    default:
        sStartMenuDataPtr->gfxLoadState = 0;
        return TRUE;
    }
    return FALSE;
}

static void PlaceStartMenuScrollIndicatorArrows(void)
{
    if (sStartMenuDataPtr->numVisibleMenuItems <= VISIBLE_BUTTONS)
    {
        if (sStartMenuDataPtr->scrollIndicatorArrowPairId != SPRITE_NONE)
        {
            RemoveScrollIndicatorArrowPair(sStartMenuDataPtr->scrollIndicatorArrowPairId);
            sStartMenuDataPtr->scrollIndicatorArrowPairId = SPRITE_NONE;
        }
        return;
    }

    u8 hiddenOptions = sStartMenuDataPtr->numVisibleMenuItems - VISIBLE_BUTTONS;

    if (sStartMenuDataPtr->scrollIndicatorArrowPairId == SPRITE_NONE)
    {
        sStartMenuDataPtr->scrollIndicatorArrowPairId = AddScrollIndicatorArrowPairParameterized(
            2,
            171,
            20,
            140,
            hiddenOptions,
            110,
            110,
            &sStartMenuDataPtr->scrollOffset
        );
    }
}

static void RemoveStartMenuScrollIndicatorArrows(void)
{
    if (sStartMenuDataPtr->scrollIndicatorArrowPairId != SPRITE_NONE)
    {
        RemoveScrollIndicatorArrowPair(sStartMenuDataPtr->scrollIndicatorArrowPairId);
        sStartMenuDataPtr->scrollIndicatorArrowPairId = SPRITE_NONE;
    }
}

static void StartMenu_GenerateVisibleMenuIndices(void)
{
    u8 i, count = 0;

    for (i = 0; i < TOTAL_MENU_OPTIONS; i++)
    {
        switch (i)
        {
        case START_MENU_POKEDEX:
            if (!FlagGet(FLAG_SYS_POKEDEX_GET))
                continue;
            break;
        case START_MENU_POKEMON:
            if (!FlagGet(FLAG_SYS_POKEMON_GET))
                continue;
            break;
        case START_MENU_QUESTS:
            if (!FlagGet(FLAG_SYS_QUEST_MENU_GET))
                continue;
            break;
        case START_MENU_DEXNAV:
            if (!FlagGet(FLAG_SYS_DEXNAV_GET))
                continue;
            break;
        default:
            break;
        }

        sStartMenuDataPtr->visibleMenuIndices[count++] = i;
    }

    sStartMenuDataPtr->numVisibleMenuItems = count;
}

static void StartMenu_CreateButtons(void)
{
    u8 i;
    for (i = 0; i < VISIBLE_BUTTONS; i++)
    {
        if (i + sStartMenuDataPtr->scrollOffset >= sStartMenuDataPtr->numVisibleMenuItems)
            break;

        u8 menuIndex = sStartMenuDataPtr->visibleMenuIndices[sStartMenuDataPtr->scrollOffset + i];
        const struct SpriteTemplate *template;

        if (menuIndex < START_MENU_GROUP_B_START)
            template = &sSpriteTemplate_MenuButtonA;
        else
            template = &sSpriteTemplate_MenuButtonB;

        sStartMenuDataPtr->MenuButtonSpriteIds[i * 2] =
            CreateSprite(template,
                         START_MENU_BUTTON_X_1,
                         START_MENU_BUTTON_Y_START + i * START_MENU_BUTTON_Y_SPACING,
                         0);

        sStartMenuDataPtr->MenuButtonSpriteIds[i * 2 + 1] =
            CreateSprite(template,
                         START_MENU_BUTTON_X_2,
                         START_MENU_BUTTON_Y_START + i * START_MENU_BUTTON_Y_SPACING,
                         0);
    }
}

static void StartMenu_UpdateVisibleButtons(void)
{
    u8 i, menuIndex;
    for (i = 0; i < VISIBLE_BUTTONS; i++)
    {
        if (i + sStartMenuDataPtr->scrollOffset >= sStartMenuDataPtr->numVisibleMenuItems)
            break;

        menuIndex = sStartMenuDataPtr->visibleMenuIndices[sStartMenuDataPtr->scrollOffset + i];

        u8 leftId = sStartMenuDataPtr->MenuButtonSpriteIds[i * 2];
        u8 rightId = sStartMenuDataPtr->MenuButtonSpriteIds[i * 2 + 1];

        const struct SpriteTemplate *expectedTemplate =
            (menuIndex < START_MENU_GROUP_B_START)
                ? &sSpriteTemplate_MenuButtonA
                : &sSpriteTemplate_MenuButtonB;

        bool8 recreateLeft = (leftId == SPRITE_NONE || gSprites[leftId].template != expectedTemplate);
        bool8 recreateRight = (rightId == SPRITE_NONE || gSprites[rightId].template != expectedTemplate);

        if (recreateLeft)
        {
            if (leftId != SPRITE_NONE)
                DestroySprite(&gSprites[leftId]);

            sStartMenuDataPtr->MenuButtonSpriteIds[i * 2] =
                CreateSprite(expectedTemplate,
                             START_MENU_BUTTON_X_1,
                             START_MENU_BUTTON_Y_START + i * START_MENU_BUTTON_Y_SPACING,
                             0);
        }
        else
        {
            gSprites[leftId].y = START_MENU_BUTTON_Y_START + i * START_MENU_BUTTON_Y_SPACING;
        }

        if (recreateRight)
        {
            if (rightId != SPRITE_NONE)
                DestroySprite(&gSprites[rightId]);

            sStartMenuDataPtr->MenuButtonSpriteIds[i * 2 + 1] =
                CreateSprite(expectedTemplate,
                             START_MENU_BUTTON_X_2,
                             START_MENU_BUTTON_Y_START + i * START_MENU_BUTTON_Y_SPACING,
                             0);
        }
        else
        {
            gSprites[rightId].y = START_MENU_BUTTON_Y_START + i * START_MENU_BUTTON_Y_SPACING;
        }

        u8 animIdx = (menuIndex < START_MENU_GROUP_B_START)
            ? menuIndex
            : menuIndex - START_MENU_GROUP_B_START;

        StartSpriteAnim(&gSprites[sStartMenuDataPtr->MenuButtonSpriteIds[i * 2]],
                        (menuIndex < START_MENU_GROUP_B_START)
                            ? sButtonAnimIndicesA[animIdx][0]
                            : sButtonAnimIndicesB[animIdx][0]);

        StartSpriteAnim(&gSprites[sStartMenuDataPtr->MenuButtonSpriteIds[i * 2 + 1]],
                        (menuIndex < START_MENU_GROUP_B_START)
                            ? sButtonAnimIndicesA[animIdx][1]
                            : sButtonAnimIndicesB[animIdx][1]);
    }
}

static void DestroyAllMenuButtons(void)
{
    for (int i = 0; i < START_MENU_BUTTON_A_COUNT; i++)
    {
        if (sStartMenuDataPtr->MenuButtonSpriteIds[i] != SPRITE_NONE)
        {
            DestroySprite(&gSprites[sStartMenuDataPtr->MenuButtonSpriteIds[i]]);
            sStartMenuDataPtr->MenuButtonSpriteIds[i] = SPRITE_NONE;
        }
    }
    
    for (int i = 0; i < START_MENU_BUTTON_B_COUNT; i++)
    {
        if (sStartMenuDataPtr->MenuButtonSpriteIds[i] != SPRITE_NONE)
        {
            DestroySprite(&gSprites[sStartMenuDataPtr->MenuButtonSpriteIds[i]]);
            sStartMenuDataPtr->MenuButtonSpriteIds[i] = SPRITE_NONE;
        }
    }
}

static void StartMenu_InitWindows(void)
{
    InitWindows(sStartMenuWindowTemplates);
    DeactivateAllTextPrinters();
    ScheduleBgCopyTilemapToVram(0);
    
    FillWindowPixelBuffer(WINDOW_HP_BARS, PIXEL_FILL(TEXT_COLOR_TRANSPARENT));
    PutWindowTilemap(WINDOW_HP_BARS);
    CopyWindowToVram(WINDOW_HP_BARS, COPYWIN_FULL);

    FillWindowPixelBuffer(WINDOW_TOP_BAR, PIXEL_FILL(TEXT_COLOR_TRANSPARENT));
    PutWindowTilemap(WINDOW_TOP_BAR);
    CopyWindowToVram(WINDOW_TOP_BAR, COPYWIN_FULL);

    FillWindowPixelBuffer(WINDOW_BOTTOM_BAR, PIXEL_FILL(TEXT_COLOR_TRANSPARENT));
    PutWindowTilemap(WINDOW_BOTTOM_BAR);
    CopyWindowToVram(WINDOW_BOTTOM_BAR, COPYWIN_FULL);
    
    ScheduleBgCopyTilemapToVram(2);
}

static const u8 sText_ConfirmSave[] = _("Confirm Save");
static const u8 sText_CancelSave[] = _("Cancel");
static const u8 sA_ButtonGfx[] = INCBIN_U8("graphics/start_menu/a_button.4bpp");
static const u8 sB_ButtonGfx[] = INCBIN_U8("graphics/start_menu/b_button.4bpp");

static void PrintSaveConfirmToWindow()
{
    const u8 *str = sText_ConfirmSave;
    const u8 *str2 = sText_CancelSave;
    u8 sConfirmTextColors[] = {TEXT_COLOR_TRANSPARENT, 2, 9};
    u8 x = 62;
    u8 x2 = x + 101;
    u8 y = 0;
    
    FillWindowPixelBuffer(WINDOW_BOTTOM_BAR, PIXEL_FILL(5));
    BlitBitmapToWindow(WINDOW_BOTTOM_BAR, sA_ButtonGfx, x - 11, 4, 8, 8);
    AddTextPrinterParameterized4(WINDOW_BOTTOM_BAR, 1, x, y, 0, 0, sConfirmTextColors, 0xFF, str);
    BlitBitmapToWindow(WINDOW_BOTTOM_BAR, sB_ButtonGfx, x2 - 11, 4, 8, 8);
    AddTextPrinterParameterized4(WINDOW_BOTTOM_BAR, 1, x2, y, 0, 0, sConfirmTextColors, 0xFF, str2);
    PutWindowTilemap(WINDOW_BOTTOM_BAR);
    CopyWindowToVram(WINDOW_BOTTOM_BAR, COPYWIN_FULL);
}

static const u8 sText_Sunday[] = _("Sunday");
static const u8 sText_Monday[] = _("Monday");
static const u8 sText_Tuesday[] = _("Tuesday");
static const u8 sText_Wednesday[] = _("Wednesday");
static const u8 sText_Thursday[] = _("Thursday");
static const u8 sText_Friday[] = _("Friday");
static const u8 sText_Saturday[] = _("Saturday");

static const u8 * const sDayOfWeekStrings[7] =
{
    sText_Sunday,
    sText_Monday,
    sText_Tuesday,
    sText_Wednesday,
    sText_Thursday,
    sText_Friday,
    sText_Saturday,
};

static const u8 sText_AM[] = _("AM");
static const u8 sText_PM[] = _("PM");

static void PrintMapNameAndTime(void)
{
    u8 mapDisplayHeader[24];
    u8 *withoutPrefixPtr;
    u8 x;
    const u8 *str, *suffix = NULL;
    u8 sTimeTextColors[] = {TEXT_COLOR_TRANSPARENT, 2, 9};

    u16 hours;
    u16 minutes;
    u16 dayOfWeek;
    s32 width;
    u32 y, totalWidth;

    FillWindowPixelBuffer(WINDOW_TOP_BAR, PIXEL_FILL(TEXT_COLOR_TRANSPARENT));

    withoutPrefixPtr = &(mapDisplayHeader[3]);
    GetMapName(withoutPrefixPtr, gMapHeader.regionMapSectionId, 0);
    x = GetStringRightAlignXOffset(FONT_NORMAL, withoutPrefixPtr, 80);
    mapDisplayHeader[0] = EXT_CTRL_CODE_BEGIN;
    mapDisplayHeader[1] = EXT_CTRL_CODE_HIGHLIGHT;
    mapDisplayHeader[2] = TEXT_COLOR_TRANSPARENT;
    AddTextPrinterParameterized3(WINDOW_TOP_BAR, FONT_NORMAL, x + 148, 0, sTimeTextColors, TEXT_SKIP_DRAW, mapDisplayHeader); // Print Map Name

    RtcCalcLocalTime();

    hours = gLocalTime.hours;

    if (gSaveBlock4Ptr->optionsClockMode == OPTIONS_CLOCK_MODE_12HR)
    {
        if (gLocalTime.hours < 12)
        {
            hours = (gLocalTime.hours == 0) ? 12 : gLocalTime.hours;
            suffix = sText_AM;
        }
        else if (gLocalTime.hours == 12)
        {
            hours = 12;
            suffix = sText_PM;
        }
        else
        {
            hours = gLocalTime.hours - 12;
            suffix = sText_PM;
        }
    }

    minutes = gLocalTime.minutes;
    dayOfWeek = gLocalTime.days % 7;
    if (hours > 999)
        hours = 999;
    if (minutes > 59)
        minutes = 59;
    width = GetStringWidth(FONT_NORMAL, gText_Colon2, 0);
    x = 96;
    y = 0;

    totalWidth = width + 30;
    x -= totalWidth;

    str = sDayOfWeekStrings[dayOfWeek];

    AddTextPrinterParameterized3(WINDOW_TOP_BAR, FONT_NORMAL, 10, y, sTimeTextColors, TEXT_SKIP_DRAW, str); // print day of week
    ConvertIntToDecimalStringN(gStringVar4, hours, STR_CONV_MODE_RIGHT_ALIGN, 3);
    AddTextPrinterParameterized3(WINDOW_TOP_BAR, FONT_NORMAL, x, y, sTimeTextColors, TEXT_SKIP_DRAW, gStringVar4); // these three print the time, you can put the colon to only print half the time to flash it if you want
    x += 18;
    AddTextPrinterParameterized3(WINDOW_TOP_BAR, FONT_NORMAL, x, y, sTimeTextColors, TEXT_SKIP_DRAW, gText_Colon2);
    x += width;
    ConvertIntToDecimalStringN(gStringVar4, minutes, STR_CONV_MODE_LEADING_ZEROS, 2);
    AddTextPrinterParameterized3(WINDOW_TOP_BAR, FONT_NORMAL, x, y, sTimeTextColors, TEXT_SKIP_DRAW, gStringVar4);

    if (suffix != NULL)
    {
        width = GetStringWidth(FONT_NORMAL, gStringVar4, 0) + 3; // CHAR_SPACE is 3 pixels wide
        x += width;
        StringExpandPlaceholders(gStringVar4, suffix);
        AddTextPrinterParameterized3(WINDOW_TOP_BAR, FONT_NORMAL, x, y, sTimeTextColors, TEXT_SKIP_DRAW, gStringVar4);
    }

    PutWindowTilemap(WINDOW_TOP_BAR);
    CopyWindowToVram(WINDOW_TOP_BAR, COPYWIN_FULL);
}

void Task_OpenPokedexFromStartMenu(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        StartMenu_FreeResources();
        IncrementGameStat(GAME_STAT_CHECKED_POKEDEX);
        PlayRainStoppingSoundEffect();
        CleanupOverworldWindowsAndTilemaps();
        SetMainCallback2(CB2_OpenPokedex);
        DestroyTask(taskId);
    }
}

void Task_OpenPokemonPartyFromStartMenu(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        StartMenu_FreeResources();
        PlayRainStoppingSoundEffect();
        CleanupOverworldWindowsAndTilemaps();
        SetMainCallback2(CB2_PartyMenuFromStartMenu);
    }
}

void Task_OpenBagFromStartMenu(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        StartMenu_FreeResources();
        PlayRainStoppingSoundEffect();
        CleanupOverworldWindowsAndTilemaps();
        SetMainCallback2(CB2_BagMenuFromStartMenu);
    }
}

void Task_OpenTrainerCardFromStartMenu(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        DmaClearLarge16(3, (void *)VRAM, VRAM_SIZE, 0x1000)
        StartMenu_FreeResources();
        PlayRainStoppingSoundEffect();
        CleanupOverworldWindowsAndTilemaps();

        if (FlagGet(FLAG_SYS_FRONTIER_PASS))
            ShowFrontierPass(CB2_ReturnToStartMenu);
        else
            ShowPlayerTrainerCard(CB2_ReturnToStartMenu);
    }
}

void Task_OpenQuestsStartMenu(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        StartMenu_FreeResources();
		PlayRainStoppingSoundEffect();
		CleanupOverworldWindowsAndTilemaps();
        SetMainCallback2(CB2_OpenQuestMenuFromStartMenu);
    }
}

void Task_OpenOptionsMenuStartMenu(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        StartMenu_FreeResources();
        PlayRainStoppingSoundEffect();
        CleanupOverworldWindowsAndTilemaps();
        SetMainCallback2(CB2_InitOptionMenu); 
        gMain.savedCallback = CB2_ReturnToStartMenu;
    }
}

void Task_OpenDexNavStartMenu(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        StartMenu_FreeResources();
        PlayRainStoppingSoundEffect();
        CleanupOverworldWindowsAndTilemaps();
        SetMainCallback2(CB2_OpenDexNavFromStartMenu);
    }
}

void Task_ReturnToFieldOnSave(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        StartMenu_FreeResources();
        PlayRainStoppingSoundEffect();
        CleanupOverworldWindowsAndTilemaps();
        SetMainCallback2(CB2_ReturnToField); 
    }
}

#define sFrameToSecondTimer data[6]

void Task_HandleSaveConfirmation(u8 taskId)
{
    if(JOY_NEW(A_BUTTON)) //confirm and leave
    {
        PlaySE(SE_SELECT);
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
        gTasks[taskId].func = Task_ReturnToFieldOnSave;
        gFieldCallback = StartMenu_SaveCallback;
        return;
    }

    if(JOY_NEW(B_BUTTON)) // back to normal Menu Control
    {
        PlaySE(SE_SELECT);
        FillWindowPixelBuffer(WINDOW_BOTTOM_BAR, PIXEL_FILL(TEXT_COLOR_TRANSPARENT));
        PutWindowTilemap(WINDOW_BOTTOM_BAR);
        CopyWindowToVram(WINDOW_BOTTOM_BAR, COPYWIN_FULL);
        gTasks[taskId].func = Task_StartMenu_Main;
        return;
    }

    if(gTasks[taskId].sFrameToSecondTimer >= 60) // every 60 frames update the time
    {
        PrintMapNameAndTime();
        gTasks[taskId].sFrameToSecondTimer = 0;
    }

    gTasks[taskId].sFrameToSecondTimer++;
}

static void Task_StartMenu_Main(u8 taskId)
{
    if (JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_PC_OFF);
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
        gTasks[taskId].func = Task_StartMenu_TurnOff;
    }

    if (JOY_NEW(DPAD_UP))
    {
        if (sStartMenuDataPtr->selector_y == 0)
        {
            if (sStartMenuDataPtr->scrollOffset == 0)
            {
                // Wrap to bottom
                if (sStartMenuDataPtr->numVisibleMenuItems > VISIBLE_BUTTONS)
                    sStartMenuDataPtr->scrollOffset = sStartMenuDataPtr->numVisibleMenuItems - VISIBLE_BUTTONS;
                else
                    sStartMenuDataPtr->scrollOffset = 0;

                sStartMenuDataPtr->selector_y = min(VISIBLE_BUTTONS, sStartMenuDataPtr->numVisibleMenuItems) - 1;
            }
            else
            {
                sStartMenuDataPtr->scrollOffset--;
            }
        }
        else
        {
            sStartMenuDataPtr->selector_y--;
        }

        PlaySE(SE_SELECT);
    }

    if (JOY_NEW(DPAD_DOWN))
    {
        u8 lastSelectableIndex = sStartMenuDataPtr->numVisibleMenuItems - 1;
        u8 visibleCount = min(VISIBLE_BUTTONS, sStartMenuDataPtr->numVisibleMenuItems);

        if (sStartMenuDataPtr->selector_y == visibleCount - 1)
        {
            if (sStartMenuDataPtr->scrollOffset + sStartMenuDataPtr->selector_y >= lastSelectableIndex)
            {
                // Wrap to top
                sStartMenuDataPtr->scrollOffset = 0;
                sStartMenuDataPtr->selector_y = 0;
            }
            else
            {
                sStartMenuDataPtr->scrollOffset++;
            }
        }
        else
        {
            if (sStartMenuDataPtr->scrollOffset + sStartMenuDataPtr->selector_y + 1 > lastSelectableIndex)
            {
                sStartMenuDataPtr->scrollOffset = 0;
                sStartMenuDataPtr->selector_y = 0;
            }
            else
            {
                sStartMenuDataPtr->selector_y++;
            }
        }

        PlaySE(SE_SELECT);
    }

    if (gSelectedMenu >= sStartMenuDataPtr->numVisibleMenuItems)
    gSelectedMenu = sStartMenuDataPtr->numVisibleMenuItems - 1;

    if (gSavedSelectorY >= min(VISIBLE_BUTTONS, sStartMenuDataPtr->numVisibleMenuItems))
        gSavedSelectorY = min(VISIBLE_BUTTONS, sStartMenuDataPtr->numVisibleMenuItems) - 1;

    StartMenu_UpdateVisibleButtons();

    if (JOY_NEW(A_BUTTON))
    {
        PlaySE(SE_SELECT); // Default sound

        switch (sStartMenuDataPtr->visibleMenuIndices[sStartMenuDataPtr->scrollOffset + sStartMenuDataPtr->selector_y])
        {
            case START_MENU_POKEDEX:
                if (FlagGet(FLAG_SYS_POKEDEX_GET))
                {
                    BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
                    gTasks[taskId].func = Task_OpenPokedexFromStartMenu;
                }
                else
                {
                    PlaySE(SE_BOO);
                }
                break;

            case START_MENU_POKEMON:
                if (FlagGet(FLAG_SYS_POKEMON_GET))
                {
                    BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
                    gTasks[taskId].func = Task_OpenPokemonPartyFromStartMenu;
                }
                else
                {
                    PlaySE(SE_BOO);
                }
                break;

            case START_MENU_BAG:
                BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
                gTasks[taskId].func = Task_OpenBagFromStartMenu;
                break;

            case START_MENU_CARD:
                BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
                gTasks[taskId].func = Task_OpenTrainerCardFromStartMenu;
                break;

            case START_MENU_QUESTS:
                if (FlagGet(FLAG_SYS_QUEST_MENU_GET))
                {
                    BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
                    gTasks[taskId].func = Task_OpenQuestsStartMenu;
                }
                else
                {
                    PlaySE(SE_BOO);
                }
                break;

            case START_MENU_OPTIONS:
                BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
                gTasks[taskId].func = Task_OpenOptionsMenuStartMenu;
                break;

            case START_MENU_DEXNAV:
                if (FlagGet(FLAG_SYS_DEXNAV_GET))
                {
                    BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
                    gTasks[taskId].func = Task_OpenDexNavStartMenu;
                }
                else
                {
                    PlaySE(SE_BOO);
                }
                break;
        }
    }

    if (JOY_NEW(START_BUTTON)) // If start button pressed go to Save Confirmation Control Task
    {
        PrintSaveConfirmToWindow();
        gTasks[taskId].func = Task_HandleSaveConfirmation;
    }

    if (gTasks[taskId].sFrameToSecondTimer >= 60) // every 60 frames update the time
    {
        PrintMapNameAndTime();
        gTasks[taskId].sFrameToSecondTimer = 0;
    }
    gTasks[taskId].sFrameToSecondTimer++;
}

#undef sFrameToSecondTimer
