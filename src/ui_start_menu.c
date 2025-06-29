#include "option_menu.h"
#include "ui_start_menu.h"
#include "global.h"
#include "battle_pike.h"
#include "battle_pyramid.h"
#include "battle_pyramid_bag.h"
#include "bg.h"
#include "decompress.h"
#include "event_data.h"
#include "event_object_movement.h"
#include "event_object_lock.h"
#include "event_scripts.h"
#include "fieldmap.h"
#include "field_effect.h"
#include "field_player_avatar.h"
#include "field_specials.h"
#include "field_weather.h"
#include "field_screen_effect.h"
#include "frontier_pass.h"
#include "frontier_util.h"
#include "gpu_regs.h"
#include "international_string_util.h"
#include "item_menu.h"
#include "link.h"
#include "load_save.h"
#include "main.h"
#include "malloc.h"
#include "menu.h"
#include "new_game.h"
#include "option_menu.h"
#include "overworld.h"
#include "palette.h"
#include "party_menu.h"
#include "pokedex.h"
#include "pokenav.h"
#include "safari_zone.h"
#include "save.h"
#include "scanline_effect.h"
#include "script.h"
#include "sprite.h"
#include "sound.h"
#include "start_menu.h"
#include "strings.h"
#include "string_util.h"
#include "task.h"
#include "text.h"
#include "text_window.h"
#include "trainer_card.h"
#include "window.h"
#include "union_room.h"
#include "constants/battle_frontier.h"
#include "constants/rgb.h"
#include "constants/songs.h"
#include "rtc.h"
#include "event_object_movement.h"
#include "gba/isagbprint.h"

/* CALLBACKS */
static void SpriteCB_IconPoketch(struct Sprite* sprite);
static void SpriteCB_IconPokedex(struct Sprite* sprite);
static void SpriteCB_IconParty(struct Sprite* sprite);
static void SpriteCB_IconBag(struct Sprite* sprite);
static void SpriteCB_IconTrainerCard(struct Sprite* sprite);
static void SpriteCB_IconSave(struct Sprite* sprite);
static void SpriteCB_IconOptions(struct Sprite* sprite);
static void SpriteCB_IconFlag(struct Sprite* sprite);

/* TASKs */
static void Task_HeatStartMenu_HandleMainInput(u8 taskId);
static void Task_HeatStartMenu_SafariZone_HandleMainInput(u8 taskId);
static void Task_HandleSave(u8 taskId);

/* OTHER FUNCTIONS */
static void GetVisibleMenuEntriesForPage(u8 *outEntries, u8 *outCount);
static void HeatStartMenu_UpdateAllSpriteAnimations(void);
static void HeatStartMenu_LoadSprites(void);
static void HeatStartMenu_CreateSprites(void);
static void HeatStartMenu_DestroySprites(void);
static void HeatStartMenu_ShowLRButtons(u8 currentPage, u8 totalPages);
static void HeatStartMenu_DestroyLRButtons(void);
static void HeatStartMenu_SafariZone_CreateSprites(void);
static void HeatStartMenu_LoadBgGfx(void);
static void HeatStartMenu_ShowTimeWindow(void);
//static void HeatStartMenu_UpdateClockDisplay(void);
static void HeatStartMenu_UpdateMenuName(void);
static u8 RunSaveCallback(void);
static u8 SaveDoSaveCallback(void);
static void HideSaveInfoWindow(void);
static void HideSaveMessageWindow(void);
static u8 SaveOverwriteInputCallback(void);
static u8 SaveConfirmOverwriteDefaultNoCallback(void);
//static u8 SaveConfirmOverwriteCallback(void);
static void ShowSaveMessage(const u8 *message, u8 (*saveCallback)(void));
static u8 SaveFileExistsCallback(void);
static u8 SaveSavingMessageCallback(void);
static u8 SaveConfirmInputCallback(void);
static u8 SaveYesNoCallback(void);
static void ShowSaveInfoWindow(void);
static u8 SaveConfirmSaveCallback(void);
static void InitSave(void);
static u8 GetTotalPageCount(void);

/* ENUMs */
enum MENU {
  MENU_POKEDEX,
  MENU_PARTY,
  MENU_BAG,
  MENU_POKETCH,
  MENU_TRAINER_CARD,
  MENU_SAVE,
  MENU_OPTIONS,
  MENU_FLAG,
};

enum FLAG_VALUES {
  FLAG_VALUE_NOT_SET,
  FLAG_VALUE_SET,
};

enum SAVE_STATES {
  SAVE_IN_PROGRESS,
  SAVE_SUCCESS,
  SAVE_CANCELED,
  SAVE_ERROR
};

/* STRUCTs */
struct HeatStartMenu {
    MainCallback savedCallback;
    u32 loadState;
    u32 sStartClockWindowId;
    u32 sMenuNameWindowId;
    u32 sSafariBallsWindowId;
    u32 flag; // some u32 holding values for controlling the sprite anims and lifetime
    
    u32 spriteIdPoketch;
    u32 spriteIdPokedex;
    u32 spriteIdParty;
    u32 spriteIdBag;
    u32 spriteIdTrainerCard;
    u32 spriteIdSave;
    u32 spriteIdOptions;
    u32 spriteIdFlag;
    u32 spriteIdLButton;
    u32 spriteIdRButton;

    u8 page;
    u8 totalPages;
};

static EWRAM_DATA struct HeatStartMenu *sHeatStartMenu = NULL;
static EWRAM_DATA u8 menuSelected = 0;
static EWRAM_DATA u8 sLastMenuSelected = 0;
static EWRAM_DATA u8 sLastMenuPage = 0;
static EWRAM_DATA u8 sLastVisibleOptionsCount = 0;
static EWRAM_DATA u8 (*sSaveDialogCallback)(void) = NULL;
static EWRAM_DATA u8 sSaveDialogTimer = 0;
static EWRAM_DATA u8 sSaveInfoWindowId = 0;

// --BG-GFX--
static const u32 sStartMenuTiles[] = INCBIN_U32("graphics/start_menu/bg.4bpp.lz");
static const u32 sStartMenuTilemap[] = INCBIN_U32("graphics/start_menu/bg.bin.lz");
static const u32 sStartMenuTilemapSafari[] = INCBIN_U32("graphics/start_menu/bg_safari.bin.lz");
static const u16 sStartMenuPalette[] = INCBIN_U16("graphics/start_menu/bg.gbapal");

//--SPRITE-GFX--
#define TAG_ICON_GFX 1234
#define TAG_BUTTON_GFX 1235
#define TAG_ICON_PAL 0x4654
#define TAG_BUTTON_PAL 0x4655

static const u32 sIconGfx[] = INCBIN_U32("graphics/start_menu/icons.4bpp.lz");
static const u32 sButtonGfx[] = INCBIN_U32("graphics/start_menu/buttons.4bpp");
static const u16 sIconPal[] = INCBIN_U16("graphics/start_menu/icons.gbapal");
static const u16 sButtonPal[] = INCBIN_U16("graphics/start_menu/buttons.gbapal");

// --PAGE-LOGIC---
#define MAX_ICONS_PER_PAGE 6

static const struct WindowTemplate sSaveInfoWindowTemplate = {
    .bg = 0,
    .tilemapLeft = 1,
    .tilemapTop = 1,
    .width = 14,
    .height = 10,
    .paletteNum = 15,
    .baseBlock = 8
};

static const struct WindowTemplate sWindowTemplate_StartClock = {
  .bg = 0, 
  .tilemapLeft = 2, 
  .tilemapTop = 17, 
  .width = 12, // If you want to shorten the dates to Sat., Sun., etc., change this to 9
  .height = 2, 
  .paletteNum = 15,
  .baseBlock = 0x30
};

static const struct WindowTemplate sWindowTemplate_MenuName = {
  .bg = 0, 
  .tilemapLeft = 16, 
  .tilemapTop = 17, 
  .width = 7, 
  .height = 2, 
  .paletteNum = 15,
  .baseBlock = 0x30 + (12*2)
};

static const struct WindowTemplate sWindowTemplate_SafariBalls = {
    .bg = 0,
    .tilemapLeft = 2,
    .tilemapTop = 1,
    .width = 7,
    .height = 4,
    .paletteNum = 15,
    .baseBlock = (0x30 + (12*2)) + (7*2)
};

static const struct SpritePalette sSpritePal_Icon[] =
{
  {sIconPal, TAG_ICON_PAL},
  {NULL},
};

static const struct SpritePalette sSpritePal_Button[] =
{
  {sButtonPal, TAG_BUTTON_PAL},
  {NULL},
};

static const struct CompressedSpriteSheet sSpriteSheet_Icon[] = 
{
  {sIconGfx, 32*512/2 , TAG_ICON_GFX},
  {NULL},
};

static const struct SpriteSheet sSpriteSheet_Button[] = 
{
  {sButtonGfx, 32*8/2 , TAG_BUTTON_GFX},
  {NULL},
};

static const struct OamData gOamIcon = {
    .y = 0,
    .affineMode = ST_OAM_AFFINE_DOUBLE,
    .objMode = 0,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(32x32),
    .x = 0,
    .matrixNum = 0,
    .size = SPRITE_SIZE(32x32),
    .tileNum = 0,
    .priority = 0,
    .paletteNum = 0,
};

static const struct OamData gOamButton = {
    .y = 0,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = 0,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(16x8),
    .x = 0,
    .matrixNum = 0,
    .size = SPRITE_SIZE(16x8),
    .tileNum = 0,
    .priority = 0,
    .paletteNum = 0,
};

static const union AnimCmd gAnimCmdPoketch_NotSelected[] = {
    ANIMCMD_FRAME(112, 0),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd gAnimCmdPoketch_Selected[] = {
    ANIMCMD_FRAME(0, 0),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd *const gIconPoketchAnim[] = {
    gAnimCmdPoketch_NotSelected,
    gAnimCmdPoketch_Selected,
};

static const union AnimCmd gAnimCmdPokedex_NotSelected[] = {
    ANIMCMD_FRAME(128, 0),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd gAnimCmdPokedex_Selected[] = {
    ANIMCMD_FRAME(16, 0),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd *const gIconPokedexAnim[] = {
    gAnimCmdPokedex_NotSelected,
    gAnimCmdPokedex_Selected,
};

static const union AnimCmd gAnimCmdParty_NotSelected[] = {
    ANIMCMD_FRAME(144, 0),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd gAnimCmdParty_Selected[] = {
    ANIMCMD_FRAME(32, 0),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd *const gIconPartyAnim[] = {
    gAnimCmdParty_NotSelected,
    gAnimCmdParty_Selected,
};

static const union AnimCmd gAnimCmdBag_NotSelected[] = {
    ANIMCMD_FRAME(160, 0),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd gAnimCmdBag_Selected[] = {
    ANIMCMD_FRAME(48, 0),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd *const gIconBagAnim[] = {
    gAnimCmdBag_NotSelected,
    gAnimCmdBag_Selected,
};

static const union AnimCmd gAnimCmdTrainerCard_NotSelected[] = {
    ANIMCMD_FRAME(176, 0),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd gAnimCmdTrainerCard_Selected[] = {
    ANIMCMD_FRAME(64, 0),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd *const gIconTrainerCardAnim[] = {
    gAnimCmdTrainerCard_NotSelected,
    gAnimCmdTrainerCard_Selected,
};

static const union AnimCmd gAnimCmdSave_NotSelected[] = {
    ANIMCMD_FRAME(192, 0),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd gAnimCmdSave_Selected[] = {
    ANIMCMD_FRAME(80, 0),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd *const gIconSaveAnim[] = {
    gAnimCmdSave_NotSelected,
    gAnimCmdSave_Selected,
};

static const union AnimCmd gAnimCmdOptions_NotSelected[] = {
    ANIMCMD_FRAME(208, 0),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd gAnimCmdOptions_Selected[] = {
    ANIMCMD_FRAME(96, 0),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd *const gIconOptionsAnim[] = {
    gAnimCmdOptions_NotSelected,
    gAnimCmdOptions_Selected,
};

static const union AnimCmd gAnimCmdFlag_NotSelected[] = {
    ANIMCMD_FRAME(240, 0),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd gAnimCmdFlag_Selected[] = {
    ANIMCMD_FRAME(224, 0),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd *const gIconFlagAnim[] = {
    gAnimCmdFlag_NotSelected,
    gAnimCmdFlag_Selected,
};

static const union AffineAnimCmd sAffineAnimIcon_NoAnim[] =
{
  AFFINEANIMCMD_FRAME(0,0, 0, 60),
  AFFINEANIMCMD_END,
};

static const union AffineAnimCmd sAffineAnimIcon_Anim[] =
{
  AFFINEANIMCMD_FRAME(20, 20, 0, 5),    // Scale big
  AFFINEANIMCMD_FRAME(-10, -10, 0, 10), // Scale smol
  AFFINEANIMCMD_FRAME(0, 0, 1, 4),      // Begin rotating

  AFFINEANIMCMD_FRAME(0, 0, -1, 4),     // Loop starts from here ; Rotate/Tilt left 
  AFFINEANIMCMD_FRAME(0, 0, 0, 2),
  AFFINEANIMCMD_FRAME(0, 0, -1, 4),
  AFFINEANIMCMD_FRAME(0, 0, 0, 2),
  AFFINEANIMCMD_FRAME(0, 0, -1, 4),

  AFFINEANIMCMD_FRAME(0, 0, 1, 4),      // Rotate/Tilt Right
  AFFINEANIMCMD_FRAME(0, 0, 0, 2),
  AFFINEANIMCMD_FRAME(0, 0, 1, 4),
  AFFINEANIMCMD_FRAME(0, 0, 0, 2),
  AFFINEANIMCMD_FRAME(0, 0, 1, 4),

  AFFINEANIMCMD_JUMP(3),
};

static const union AnimCmd sAnimCmd_L[] = {
    ANIMCMD_FRAME(0, 0),   // Start at pixel 0 (left button)
    ANIMCMD_END,
};

static const union AnimCmd sAnimCmd_R[] = {
    ANIMCMD_FRAME(2, 0),
    ANIMCMD_END,
};

static const union AnimCmd *const sAnimCmds_L[] = { sAnimCmd_L };
static const union AnimCmd *const sAnimCmds_R[] = { sAnimCmd_R };

static const union AffineAnimCmd *const sAffineAnimsIcon[] =
{   
    sAffineAnimIcon_NoAnim,
    sAffineAnimIcon_Anim,
};

static const struct SpriteTemplate gSpriteIconPoketch = {
    .tileTag = TAG_ICON_GFX,
    .paletteTag = TAG_ICON_PAL,
    .oam = &gOamIcon,
    .anims = gIconPoketchAnim,
    .images = NULL,
    .affineAnims = sAffineAnimsIcon,
    .callback = SpriteCB_IconPoketch,
};

static const struct SpriteTemplate gSpriteIconPokedex = {
    .tileTag = TAG_ICON_GFX,
    .paletteTag = TAG_ICON_PAL,
    .oam = &gOamIcon,
    .anims = gIconPokedexAnim,
    .images = NULL,
    .affineAnims = sAffineAnimsIcon,
    .callback = SpriteCB_IconPokedex,
};

static const struct SpriteTemplate gSpriteIconParty = {
    .tileTag = TAG_ICON_GFX,
    .paletteTag = TAG_ICON_PAL,
    .oam = &gOamIcon,
    .anims = gIconPartyAnim,
    .images = NULL,
    .affineAnims = sAffineAnimsIcon,
    .callback = SpriteCB_IconParty,
};

static const struct SpriteTemplate gSpriteIconBag = {
    .tileTag = TAG_ICON_GFX,
    .paletteTag = TAG_ICON_PAL,
    .oam = &gOamIcon,
    .anims = gIconBagAnim,
    .images = NULL,
    .affineAnims = sAffineAnimsIcon,
    .callback = SpriteCB_IconBag,
};

static const struct SpriteTemplate gSpriteIconTrainerCard = {
    .tileTag = TAG_ICON_GFX,
    .paletteTag = TAG_ICON_PAL,
    .oam = &gOamIcon,
    .anims = gIconTrainerCardAnim,
    .images = NULL,
    .affineAnims = sAffineAnimsIcon,
    .callback = SpriteCB_IconTrainerCard,
};

static const struct SpriteTemplate gSpriteIconSave = {
    .tileTag = TAG_ICON_GFX,
    .paletteTag = TAG_ICON_PAL,
    .oam = &gOamIcon,
    .anims = gIconSaveAnim,
    .images = NULL,
    .affineAnims = sAffineAnimsIcon,
    .callback = SpriteCB_IconSave,
};

static const struct SpriteTemplate gSpriteIconOptions = {
    .tileTag = TAG_ICON_GFX,
    .paletteTag = TAG_ICON_PAL,
    .oam = &gOamIcon,
    .anims = gIconOptionsAnim,
    .images = NULL,
    .affineAnims = sAffineAnimsIcon,
    .callback = SpriteCB_IconOptions,
};

static const struct SpriteTemplate gSpriteIconFlag = {
    .tileTag = TAG_ICON_GFX,
    .paletteTag = TAG_ICON_PAL,
    .oam = &gOamIcon,
    .anims = gIconFlagAnim,
    .images = NULL,
    .affineAnims = sAffineAnimsIcon,
    .callback = SpriteCB_IconFlag,
};

static const struct SpriteTemplate gSpriteButtonL = {
    .tileTag = TAG_BUTTON_GFX,
    .paletteTag = TAG_BUTTON_PAL,
    .oam = &gOamButton,
    .anims = sAnimCmds_L,
    .images = NULL,
    .affineAnims = NULL,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteTemplate gSpriteButtonR = {
    .tileTag = TAG_BUTTON_GFX,
    .paletteTag = TAG_BUTTON_PAL,
    .oam = &gOamButton,
    .anims = sAnimCmds_R,
    .images = NULL,
    .affineAnims = NULL,
    .callback = SpriteCallbackDummy,
};

static void SpriteCB_IconPoketch(struct Sprite* sprite) {
    if (menuSelected == MENU_POKETCH) {
        if (sprite->data[0] == 0)  // just selected
        {
            sprite->data[0] = 1;
            StartSpriteAnim(sprite, 1);
            StartSpriteAffineAnim(sprite, 1);
        }
    } else {
        if (sprite->data[0] == 1)  // just deselected
        {
            sprite->data[0] = 0;
            StartSpriteAnim(sprite, 0);
            StartSpriteAffineAnim(sprite, 0);
        }
    }
}

static void SpriteCB_IconPokedex(struct Sprite* sprite) {
    if (menuSelected == MENU_POKEDEX) {
        if (sprite->data[0] == 0)
        {
            sprite->data[0] = 1;
            StartSpriteAnim(sprite, 1);
            StartSpriteAffineAnim(sprite, 1);
        }
    } else {
        if (sprite->data[0] == 1)
        {
            sprite->data[0] = 0;
            StartSpriteAnim(sprite, 0);
            StartSpriteAffineAnim(sprite, 0);
        }
    }
}

static void SpriteCB_IconParty(struct Sprite* sprite) {
    if (menuSelected == MENU_PARTY) {
        if (sprite->data[0] == 0)
        {
            sprite->data[0] = 1;
            StartSpriteAnim(sprite, 1);
            StartSpriteAffineAnim(sprite, 1);
        }
    } else {
        if (sprite->data[0] == 1)
        {
            sprite->data[0] = 0;
            StartSpriteAnim(sprite, 0);
            StartSpriteAffineAnim(sprite, 0);
        }
    }
}

static void SpriteCB_IconBag(struct Sprite* sprite) {
    if (menuSelected == MENU_BAG) {
        if (sprite->data[0] == 0)
        {
            sprite->data[0] = 1;
            StartSpriteAnim(sprite, 1);
            StartSpriteAffineAnim(sprite, 1);
        }
    } else {
        if (sprite->data[0] == 1)
        {
            sprite->data[0] = 0;
            StartSpriteAnim(sprite, 0);
            StartSpriteAffineAnim(sprite, 0);
        }
    }
}

static void SpriteCB_IconTrainerCard(struct Sprite* sprite) {
    if (menuSelected == MENU_TRAINER_CARD) {
        if (sprite->data[0] == 0)
        {
            sprite->data[0] = 1;
            StartSpriteAnim(sprite, 1);
            StartSpriteAffineAnim(sprite, 1);
        }
    } else {
        if (sprite->data[0] == 1)
        {
            sprite->data[0] = 0;
            StartSpriteAnim(sprite, 0);
            StartSpriteAffineAnim(sprite, 0);
        }
    }
}

static void SpriteCB_IconSave(struct Sprite* sprite) {
    if (menuSelected == MENU_SAVE) {
        if (sprite->data[0] == 0)
        {
            sprite->data[0] = 1;
            StartSpriteAnim(sprite, 1);
            StartSpriteAffineAnim(sprite, 1);
        }
    } else {
        if (sprite->data[0] == 1)
        {
            sprite->data[0] = 0;
            StartSpriteAnim(sprite, 0);
            StartSpriteAffineAnim(sprite, 0);
        }
    }
}

static void SpriteCB_IconOptions(struct Sprite* sprite) {
    if (menuSelected == MENU_OPTIONS) {
        if (sprite->data[0] == 0)
        {
            sprite->data[0] = 1;
            StartSpriteAnim(sprite, 1);
            StartSpriteAffineAnim(sprite, 1);
        }
    } else {
        if (sprite->data[0] == 1)
        {
            sprite->data[0] = 0;
            StartSpriteAnim(sprite, 0);
            StartSpriteAffineAnim(sprite, 0);
        }
    }
}

static void SpriteCB_IconFlag(struct Sprite* sprite) {
  if (menuSelected == MENU_FLAG && sHeatStartMenu->flag == FLAG_VALUE_NOT_SET) {
    sHeatStartMenu->flag = FLAG_VALUE_SET;
    StartSpriteAnim(sprite, 1);
    StartSpriteAffineAnim(sprite, 1);
  } else if (menuSelected != MENU_FLAG) {
    StartSpriteAnim(sprite, 0);
    StartSpriteAffineAnim(sprite, 0);
  } 
}

// If you want to shorten the dates to Sat., Sun., etc., change this to 70
#define CLOCK_WINDOW_WIDTH 100

static const u8 gText_Friday[]    = _("Fri,");
static const u8 gText_Saturday[]  = _("Sat,");
static const u8 gText_Sunday[]    = _("Sun,");
static const u8 gText_Monday[]    = _("Mon,");
static const u8 gText_Tuesday[]   = _("Tue,");
static const u8 gText_Wednesday[] = _("Wed,");
static const u8 gText_Thursday[]  = _("Thu,");

static const u8 *const gDayNameStringsTable[] =
{
    gText_Friday,
    gText_Saturday,
    gText_Sunday,
    gText_Monday,
    gText_Tuesday,
    gText_Wednesday,
    gText_Thursday
};

static const u8 gText_CurrentTime[]      = _("  {STR_VAR_3} {CLEAR_TO 64}{STR_VAR_1}:{STR_VAR_2}");
static const u8 gText_CurrentTimeOff[]   = _("  {STR_VAR_3} {CLEAR_TO 64}{STR_VAR_1} {STR_VAR_2}");
static const u8 gText_CurrentTimeAM[]    = _("  {STR_VAR_3} {CLEAR_TO 51}{STR_VAR_1}:{STR_VAR_2} AM");
static const u8 gText_CurrentTimeAMOff[] = _("  {STR_VAR_3} {CLEAR_TO 51}{STR_VAR_1} {STR_VAR_2} AM");
static const u8 gText_CurrentTimePM[]    = _("  {STR_VAR_3} {CLEAR_TO 51}{STR_VAR_1}:{STR_VAR_2} PM");
static const u8 gText_CurrentTimePMOff[] = _("  {STR_VAR_3} {CLEAR_TO 51}{STR_VAR_1} {STR_VAR_2} PM");

static void SetSelectedMenu(void) {
  if (FlagGet(FLAG_SYS_POKENAV_GET) == TRUE) {
    menuSelected = MENU_POKETCH;
  } else if (FlagGet(FLAG_SYS_POKEDEX_GET) == TRUE) {
    menuSelected = MENU_POKEDEX;
  } else if (FlagGet(FLAG_SYS_POKEMON_GET) == TRUE) {
    menuSelected = MENU_PARTY;
  } else {
    menuSelected = MENU_BAG;
  }
}

static void ShowSafariBallsWindow(void)
{
    sHeatStartMenu->sSafariBallsWindowId = AddWindow(&sWindowTemplate_SafariBalls);
    FillWindowPixelBuffer(sHeatStartMenu->sSafariBallsWindowId, PIXEL_FILL(TEXT_COLOR_WHITE));
    PutWindowTilemap(sHeatStartMenu->sSafariBallsWindowId);
    ConvertIntToDecimalStringN(gStringVar1, gNumSafariBalls, STR_CONV_MODE_RIGHT_ALIGN, 2);
    StringExpandPlaceholders(gStringVar4, gText_SafariBallStock);
    AddTextPrinterParameterized(sHeatStartMenu->sSafariBallsWindowId, FONT_NARROW, gStringVar4, 0, 1, TEXT_SKIP_DRAW, NULL);
    CopyWindowToVram(sHeatStartMenu->sSafariBallsWindowId, COPYWIN_GFX);
}

void HeatStartMenu_Init(void)
{
    if (!IsOverworldLinkActive())
    {
        FreezeObjectEvents();
        PlayerFreeze();
        StopPlayerAvatar();
    }

    LockPlayerFieldControls();

    if (sHeatStartMenu == NULL)
    {
        sHeatStartMenu = AllocZeroed(sizeof(struct HeatStartMenu));
        if (sHeatStartMenu == NULL)
        {
            SetMainCallback2(CB2_ReturnToFieldWithOpenMenu);
            return;
        }
    }

    *sHeatStartMenu = (struct HeatStartMenu){
        .savedCallback = CB2_ReturnToFieldWithOpenMenu,
        .spriteIdPokedex = SPRITE_NONE,
        .spriteIdParty = SPRITE_NONE,
        .spriteIdBag = SPRITE_NONE,
        .spriteIdPoketch = SPRITE_NONE,
        .spriteIdTrainerCard = SPRITE_NONE,
        .spriteIdSave = SPRITE_NONE,
        .spriteIdOptions = SPRITE_NONE,
        .spriteIdFlag = SPRITE_NONE,
        .spriteIdLButton = SPRITE_NONE,
        .spriteIdRButton = SPRITE_NONE,
    };

    // Compute total pages first so it's always valid
    sHeatStartMenu->totalPages = GetTotalPageCount();

    // Determine visible entries
    u8 firstVisibleOption, visibleCount;
    GetVisibleMenuEntriesForPage(&firstVisibleOption, &visibleCount);

    if (visibleCount > sLastVisibleOptionsCount)
    {
        sHeatStartMenu->page = 0;
        menuSelected = firstVisibleOption;
    }
    else if (sLastMenuSelected != 255)
    {
        menuSelected = sLastMenuSelected;

        const u8 menuOrder[] = {
            MENU_POKEDEX, MENU_PARTY, MENU_BAG, MENU_POKETCH,
            MENU_TRAINER_CARD, MENU_SAVE, MENU_OPTIONS
        };

        u8 visibleIndex = 0;
        for (u8 i = 0; i < ARRAY_COUNT(menuOrder); i++)
        {
            bool32 visible = TRUE;
            switch (menuOrder[i])
            {
                case MENU_POKEDEX: visible = FlagGet(FLAG_SYS_POKEDEX_GET); break;
                case MENU_PARTY:   visible = FlagGet(FLAG_SYS_POKEMON_GET); break;
                case MENU_POKETCH: visible = FlagGet(FLAG_SYS_POKENAV_GET); break;
            }

            if (!visible) continue;

            if (menuOrder[i] == sLastMenuSelected)
            {
                sHeatStartMenu->page = visibleIndex / MAX_ICONS_PER_PAGE;
                break;
            }

            visibleIndex++;
        }
    }
    else
    {
        sHeatStartMenu->page = 0;
        menuSelected = firstVisibleOption;
    }

    sLastVisibleOptionsCount = visibleCount;

    HeatStartMenu_LoadSprites();
    HeatStartMenu_LoadBgGfx();
    HeatStartMenu_ShowTimeWindow();

    sHeatStartMenu->sMenuNameWindowId = AddWindow(&sWindowTemplate_MenuName);
    HeatStartMenu_UpdateMenuName();

    if (GetSafariZoneFlag())
    {
        if (menuSelected == 255 || menuSelected == MENU_POKETCH || menuSelected == MENU_SAVE)
            menuSelected = MENU_FLAG;

        sHeatStartMenu->page = 0;
        HeatStartMenu_SafariZone_CreateSprites();
        ShowSafariBallsWindow();
        CreateTask(Task_HeatStartMenu_SafariZone_HandleMainInput, 0);
    }
    else
    {
        if (!FlagGet(FLAG_SYS_POKENAV_GET) && menuSelected == 0)
            menuSelected = 255;

        if (menuSelected == MENU_FLAG)
            menuSelected = MENU_POKEDEX;

        if (menuSelected == 255)
            SetSelectedMenu();

        HeatStartMenu_CreateSprites();
        CreateTask(Task_HeatStartMenu_HandleMainInput, 0);
    }

    HeatStartMenu_ShowLRButtons(sHeatStartMenu->page, sHeatStartMenu->totalPages);
}

static void HeatStartMenu_UpdateAllSpriteAnimations(void)
{
    if (!sHeatStartMenu)
        return;

    u32 spriteIds[] = {
        sHeatStartMenu->spriteIdPokedex,
        sHeatStartMenu->spriteIdParty,
        sHeatStartMenu->spriteIdBag,
        sHeatStartMenu->spriteIdPoketch,
        sHeatStartMenu->spriteIdTrainerCard,
        sHeatStartMenu->spriteIdSave,
        sHeatStartMenu->spriteIdOptions
    };

    for (u32 i = 0; i < ARRAY_COUNT(spriteIds); i++)
    {
        if (spriteIds[i] != SPRITE_NONE)
        {
            struct Sprite* sprite = &gSprites[spriteIds[i]];
            if (sprite->callback)
                sprite->callback(sprite);
        }
    }
}

static void HeatStartMenu_LoadSprites(void) {
    LoadSpriteSheet(sSpriteSheet_Button);
    LoadSpritePalette(sSpritePal_Button);
    u32 index;
    LoadSpritePalette(sSpritePal_Icon);
    index = IndexOfSpritePaletteTag(TAG_ICON_PAL);
    LoadPalette(sIconPal, OBJ_PLTT_ID(index), PLTT_SIZE_4BPP); 
    LoadCompressedSpriteSheet(sSpriteSheet_Icon);
}

static void HeatStartMenu_CreateSprites(void)
{
    const struct {
        const struct SpriteTemplate *template;
        u16 flag;
        u32 *spriteId;
        u8 menuId;
    } iconEntries[] = {
        {&gSpriteIconPokedex,     FLAG_SYS_POKEDEX_GET,     &sHeatStartMenu->spriteIdPokedex,     MENU_POKEDEX},
        {&gSpriteIconParty,       FLAG_SYS_POKEMON_GET,     &sHeatStartMenu->spriteIdParty,       MENU_PARTY},
        {&gSpriteIconBag,         0,                        &sHeatStartMenu->spriteIdBag,         MENU_BAG},
        {&gSpriteIconPoketch,     FLAG_SYS_POKENAV_GET,     &sHeatStartMenu->spriteIdPoketch,     MENU_POKETCH},
        {&gSpriteIconTrainerCard, 0,                        &sHeatStartMenu->spriteIdTrainerCard, MENU_TRAINER_CARD},
        {&gSpriteIconSave,        0,                        &sHeatStartMenu->spriteIdSave,        MENU_SAVE},
        {&gSpriteIconOptions,     0,                        &sHeatStartMenu->spriteIdOptions,     MENU_OPTIONS},
    };

    u32 x = 224;
    u32 yBase = 12;
    u32 ySpacing = 25;

    u32 totalUnlocked = 0;
    for (u32 i = 0; i < ARRAY_COUNT(iconEntries); i++) {
        if (iconEntries[i].flag == 0 || FlagGet(iconEntries[i].flag))
            totalUnlocked++;
    }

    sHeatStartMenu->totalPages = (totalUnlocked + MAX_ICONS_PER_PAGE - 1) / MAX_ICONS_PER_PAGE;
    if (sHeatStartMenu->page >= sHeatStartMenu->totalPages)
        sHeatStartMenu->page = 0;

    u32 skip = sHeatStartMenu->page * MAX_ICONS_PER_PAGE;
    u32 shown = 0;

    for (u32 i = 0; i < ARRAY_COUNT(iconEntries); i++) {
        if (iconEntries[i].flag && !FlagGet(iconEntries[i].flag))
            continue;

        if (skip > 0) {
            skip--;
            continue;
        }

        if (shown >= MAX_ICONS_PER_PAGE)
            break;

        u32 y = yBase + shown * ySpacing;
        if (iconEntries[i].template == &gSpriteIconPoketch)
            y += 2;

        if (iconEntries[i].template == &gSpriteIconTrainerCard)
            y += 2;

        u8 spriteId = CreateSprite(iconEntries[i].template, x, y, 0);
        if (spriteId != MAX_SPRITES) {
            *iconEntries[i].spriteId = spriteId;
            struct Sprite *sprite = &gSprites[spriteId];
            sprite->data[0] = (menuSelected == iconEntries[i].menuId) ? 1 : 0;
            StartSpriteAnim(sprite, sprite->data[0]);
            StartSpriteAffineAnim(sprite, sprite->data[0]);
        }

        shown++;
    }
}

static void HeatStartMenu_DestroySprites(void)
{
#define DESTROY_SPRITE_SAFE(id) \
    do { \
        if ((id) != SPRITE_NONE) { \
            FreeSpriteOamMatrix(&gSprites[id]); \
            DestroySprite(&gSprites[id]); \
            (id) = SPRITE_NONE; \
        } \
    } while(0)

    DESTROY_SPRITE_SAFE(sHeatStartMenu->spriteIdPokedex);
    DESTROY_SPRITE_SAFE(sHeatStartMenu->spriteIdParty);
    DESTROY_SPRITE_SAFE(sHeatStartMenu->spriteIdBag);
    DESTROY_SPRITE_SAFE(sHeatStartMenu->spriteIdPoketch);
    DESTROY_SPRITE_SAFE(sHeatStartMenu->spriteIdTrainerCard);
    DESTROY_SPRITE_SAFE(sHeatStartMenu->spriteIdSave);
    DESTROY_SPRITE_SAFE(sHeatStartMenu->spriteIdOptions);
    DESTROY_SPRITE_SAFE(sHeatStartMenu->spriteIdFlag);

    HeatStartMenu_DestroyLRButtons();

#undef DESTROY_SPRITE_SAFE
}

static void HeatStartMenu_ShowLRButtons(u8 currentPage, u8 totalPages)
{
    // Destroy old buttons if exist
    if (sHeatStartMenu->spriteIdLButton != SPRITE_NONE)
    {
        DestroySprite(&gSprites[sHeatStartMenu->spriteIdLButton]);
        sHeatStartMenu->spriteIdLButton = SPRITE_NONE;
    }
    if (sHeatStartMenu->spriteIdRButton != SPRITE_NONE)
    {
        DestroySprite(&gSprites[sHeatStartMenu->spriteIdRButton]);
        sHeatStartMenu->spriteIdRButton = SPRITE_NONE;
    }

    // Load gfx and palette once
    LoadSpriteSheet(sSpriteSheet_Button);
    LoadSpritePalette(sSpritePal_Button);

    if (currentPage > 0)
    {
        u8 spriteId = CreateSprite(&gSpriteButtonL, 216, 155, 0);
        if (spriteId != MAX_SPRITES)
        {
            sHeatStartMenu->spriteIdLButton = spriteId;
            StartSpriteAnim(&gSprites[spriteId], 0);
        }
    }

    if (currentPage < totalPages - 1)
    {
        u8 spriteId = CreateSprite(&gSpriteButtonR, 232, 155, 0);
        if (spriteId != MAX_SPRITES)
        {
            sHeatStartMenu->spriteIdRButton = spriteId;
            StartSpriteAnim(&gSprites[spriteId], 0);
        }
    }
}

static void HeatStartMenu_DestroyLRButtons(void)
{
    if (sHeatStartMenu->spriteIdLButton != SPRITE_NONE)
    {
        DestroySprite(&gSprites[sHeatStartMenu->spriteIdLButton]);
        sHeatStartMenu->spriteIdLButton = SPRITE_NONE;
    }

    if (sHeatStartMenu->spriteIdRButton != SPRITE_NONE)
    {
        DestroySprite(&gSprites[sHeatStartMenu->spriteIdRButton]);
        sHeatStartMenu->spriteIdRButton = SPRITE_NONE;
    }

    FreeSpriteTilesByTag(TAG_BUTTON_GFX);
    FreeSpritePaletteByTag(TAG_BUTTON_PAL);
}

static void HeatStartMenu_SafariZone_CreateSprites(void) {
  u32 x = 224;
  u32 y1 = 14;
  u32 y2 = 38;
  u32 y3 = 60;
  u32 y4 = 84;
  u32 y5 = 109;
  u32 y6 = 130;

  sHeatStartMenu->spriteIdFlag = CreateSprite(&gSpriteIconFlag, x, y1, 0);
  sHeatStartMenu->spriteIdPokedex = CreateSprite(&gSpriteIconPokedex, x-1, y2, 0);
  sHeatStartMenu->spriteIdParty   = CreateSprite(&gSpriteIconParty, x, y3, 0);
  sHeatStartMenu->spriteIdBag     = CreateSprite(&gSpriteIconBag, x, y4, 0);
  sHeatStartMenu->spriteIdTrainerCard = CreateSprite(&gSpriteIconTrainerCard, x, y5, 0);
  sHeatStartMenu->spriteIdOptions = CreateSprite(&gSpriteIconOptions, x, y6, 0);
}

static void HeatStartMenu_LoadBgGfx(void) {
  u8* buf = GetBgTilemapBuffer(0); 
  LoadBgTilemap(0, 0, 0, 0);
  DecompressAndCopyTileDataToVram(0, sStartMenuTiles, 0, 0, 0);
  if (GetSafariZoneFlag() == FALSE) {
    LZDecompressWram(sStartMenuTilemap, buf);
  } else {
    LZDecompressWram(sStartMenuTilemapSafari, buf);
  }
  LoadPalette(gStandardMenuPalette, BG_PLTT_ID(15), PLTT_SIZE_4BPP);
  LoadPalette(sStartMenuPalette, BG_PLTT_ID(14), PLTT_SIZE_4BPP);
  ScheduleBgCopyTilemapToVram(0);
}

static void HeatStartMenu_ShowTimeWindow(void)
{
    u8 analogHour;

	RtcCalcLocalTime();
      // print window
  sHeatStartMenu->sStartClockWindowId = AddWindow(&sWindowTemplate_StartClock);
  FillWindowPixelBuffer(sHeatStartMenu->sStartClockWindowId, PIXEL_FILL(TEXT_COLOR_WHITE));
  PutWindowTilemap(sHeatStartMenu->sStartClockWindowId);
	FlagSet(FLAG_TEMP_5);

    analogHour = (gLocalTime.hours >= 13 && gLocalTime.hours <= 24) ? gLocalTime.hours - 12 : gLocalTime.hours;

	StringCopy(gStringVar3, gDayNameStringsTable[(gLocalTime.days % 7)]);
    ConvertIntToDecimalStringN(gStringVar1, gLocalTime.hours, STR_CONV_MODE_LEADING_ZEROS, 2);
	ConvertIntToDecimalStringN(gStringVar2, gLocalTime.minutes, STR_CONV_MODE_LEADING_ZEROS, 2);
	    ConvertIntToDecimalStringN(gStringVar1, analogHour, STR_CONV_MODE_LEADING_ZEROS, 2);
    
	StringExpandPlaceholders(gStringVar4, gText_CurrentTime);
        if (gLocalTime.hours >= 13 && gLocalTime.hours <= 24)
            StringExpandPlaceholders(gStringVar4, gText_CurrentTimePM); 
        else
            StringExpandPlaceholders(gStringVar4, gText_CurrentTimeAM);  
    
	AddTextPrinterParameterized(sHeatStartMenu->sStartClockWindowId, 1, gStringVar4, 0, 1, 0xFF, NULL);
	CopyWindowToVram(sHeatStartMenu->sStartClockWindowId, COPYWIN_GFX);
}

/*static void HeatStartMenu_UpdateClockDisplay(void)
{
    u8 analogHour;

	if (!FlagGet(FLAG_TEMP_5))
		return;
	RtcCalcLocalTime();
    analogHour = (gLocalTime.hours >= 13 && gLocalTime.hours <= 24) ? gLocalTime.hours - 12 : gLocalTime.hours;
    
	StringCopy(gStringVar3, gDayNameStringsTable[(gLocalTime.days % 7)]);
    ConvertIntToDecimalStringN(gStringVar1, gLocalTime.hours, STR_CONV_MODE_LEADING_ZEROS, 2);
	ConvertIntToDecimalStringN(gStringVar2, gLocalTime.minutes, STR_CONV_MODE_LEADING_ZEROS, 2);
	    ConvertIntToDecimalStringN(gStringVar1, analogHour, STR_CONV_MODE_LEADING_ZEROS, 2);
    if (gLocalTime.hours == 0)
		ConvertIntToDecimalStringN(gStringVar1, 12, STR_CONV_MODE_LEADING_ZEROS, 2);
    if (gLocalTime.hours == 12)
		ConvertIntToDecimalStringN(gStringVar1, 12, STR_CONV_MODE_LEADING_ZEROS, 2);

	if (gLocalTime.seconds % 2)
	{
        StringExpandPlaceholders(gStringVar4, gText_CurrentTime);
            if (gLocalTime.hours >= 12 && gLocalTime.hours <= 24)
                StringExpandPlaceholders(gStringVar4, gText_CurrentTimePM); 
            else
                StringExpandPlaceholders(gStringVar4, gText_CurrentTimeAM);  
    }
	else
	{
        StringExpandPlaceholders(gStringVar4, gText_CurrentTimeOff);
            if (gLocalTime.hours >= 12 && gLocalTime.hours <= 24)
                StringExpandPlaceholders(gStringVar4, gText_CurrentTimePMOff); 
            else
                StringExpandPlaceholders(gStringVar4, gText_CurrentTimeAMOff);  
    }
    
	AddTextPrinterParameterized(sHeatStartMenu->sStartClockWindowId, 1, gStringVar4, 0, 1, 0xFF, NULL);
	CopyWindowToVram(sHeatStartMenu->sStartClockWindowId, COPYWIN_GFX);
}*/

static const u8 gText_Poketch[] = _("  PokeNav");
static const u8 gText_Pokedex[] = _("  Pokédex");
static const u8 gText_Party[]   = _("    Party ");
static const u8 gText_Bag[]     = _("      Bag  ");
static const u8 gText_Trainer[] = _("   Trainer");
static const u8 gText_Save[]    = _("     Save  ");
static const u8 gText_Options[] = _("   Options");
static const u8 gText_Flag[]    = _("   Retire");

static void HeatStartMenu_UpdateMenuName(void)
{
    static const u8 sTextColors[3] = {1, 2, 3}; // background, text, shadow

    FillWindowPixelBuffer(sHeatStartMenu->sMenuNameWindowId, PIXEL_FILL(sTextColors[0]));
    PutWindowTilemap(sHeatStartMenu->sMenuNameWindowId);

    const u8 *text = NULL;
    switch (menuSelected)
    {
        case MENU_POKETCH:      text = gText_Poketch;      break;
        case MENU_POKEDEX:      text = gText_Pokedex;      break;
        case MENU_PARTY:        text = gText_Party;        break;
        case MENU_BAG:          text = gText_Bag;          break;
        case MENU_TRAINER_CARD: text = gText_Trainer;      break;
        case MENU_SAVE:         text = gText_Save;         break;
        case MENU_OPTIONS:      text = gText_Options;      break;
        case MENU_FLAG:         text = gText_Flag;         break;
    }

    if (text != NULL)
        AddTextPrinterParameterized3(sHeatStartMenu->sMenuNameWindowId, 1, 1, 0, sTextColors, 0xFF, text);

    CopyWindowToVram(sHeatStartMenu->sMenuNameWindowId, COPYWIN_GFX);
}


static void HeatStartMenu_ExitAndClearTilemap(void)
{
    u8 *buf = GetBgTilemapBuffer(0);
    u32 i;

    // Clear and remove menu windows
    FillWindowPixelBuffer(sHeatStartMenu->sMenuNameWindowId, PIXEL_FILL(TEXT_COLOR_TRANSPARENT));
    FillWindowPixelBuffer(sHeatStartMenu->sStartClockWindowId, PIXEL_FILL(TEXT_COLOR_TRANSPARENT));
    ClearWindowTilemap(sHeatStartMenu->sMenuNameWindowId);
    ClearWindowTilemap(sHeatStartMenu->sStartClockWindowId);
    CopyWindowToVram(sHeatStartMenu->sMenuNameWindowId, COPYWIN_GFX);
    CopyWindowToVram(sHeatStartMenu->sStartClockWindowId, COPYWIN_GFX);
    RemoveWindow(sHeatStartMenu->sStartClockWindowId);
    RemoveWindow(sHeatStartMenu->sMenuNameWindowId);

    if (GetSafariZoneFlag())
    {
        FillWindowPixelBuffer(sHeatStartMenu->sSafariBallsWindowId, PIXEL_FILL(TEXT_COLOR_TRANSPARENT));
        ClearWindowTilemap(sHeatStartMenu->sSafariBallsWindowId);
        CopyWindowToVram(sHeatStartMenu->sSafariBallsWindowId, COPYWIN_GFX);
        RemoveWindow(sHeatStartMenu->sSafariBallsWindowId);
    }

    // Clear background tilemap
    for (i = 0; i < 2048; i++)
        buf[i] = 0;
    ScheduleBgCopyTilemapToVram(0);

    // Helper macro to safely free/destroy a sprite by sprite ID pointer
    #define SAFE_DESTROY_SPRITE(spriteIdPtr)             \
        do {                                             \
            if (*(spriteIdPtr) != SPRITE_NONE)           \
            {                                            \
                FreeSpriteOamMatrix(&gSprites[*(spriteIdPtr)]); \
                DestroySprite(&gSprites[*(spriteIdPtr)]); \
                *(spriteIdPtr) = SPRITE_NONE;             \
            }                                            \
        } while (0)

    // Destroy sprites, checking flags and only if sprite is valid
    if (FlagGet(FLAG_SYS_POKEDEX_GET))
        SAFE_DESTROY_SPRITE(&sHeatStartMenu->spriteIdPokedex);

    if (FlagGet(FLAG_SYS_POKEMON_GET))
        SAFE_DESTROY_SPRITE(&sHeatStartMenu->spriteIdParty);

    SAFE_DESTROY_SPRITE(&sHeatStartMenu->spriteIdBag);
    SAFE_DESTROY_SPRITE(&sHeatStartMenu->spriteIdTrainerCard);
    SAFE_DESTROY_SPRITE(&sHeatStartMenu->spriteIdOptions);

    if (!GetSafariZoneFlag())
    {
        SAFE_DESTROY_SPRITE(&sHeatStartMenu->spriteIdSave);
        if (FlagGet(FLAG_SYS_POKENAV_GET))
            SAFE_DESTROY_SPRITE(&sHeatStartMenu->spriteIdPoketch);
    }
    else
    {
        SAFE_DESTROY_SPRITE(&sHeatStartMenu->spriteIdFlag);
    }

    HeatStartMenu_DestroyLRButtons();

    #undef SAFE_DESTROY_SPRITE

    // Free sprite tiles after destroying sprites
    FreeSpriteTilesByTag(TAG_ICON_GFX);

    // Free menu data struct last
    if (sHeatStartMenu != NULL)
    {
        Free(sHeatStartMenu);
        sHeatStartMenu = NULL;
    }

    ScriptUnfreezeObjectEvents();
    UnlockPlayerFieldControls();
}

static void DoCleanUpAndChangeCallback(MainCallback callback) {
  if (!gPaletteFade.active) {
    DestroyTask(FindTaskIdByFunc(Task_HeatStartMenu_HandleMainInput));
    PlayRainStoppingSoundEffect();
    HeatStartMenu_ExitAndClearTilemap();
    CleanupOverworldWindowsAndTilemaps();
    SetMainCallback2(callback);
    gMain.savedCallback = CB2_ReturnToFieldWithOpenMenu;
  }
}

static void DoCleanUpAndOpenTrainerCard(void) {
  if (!gPaletteFade.active) {
    PlayRainStoppingSoundEffect();
    HeatStartMenu_ExitAndClearTilemap();
    CleanupOverworldWindowsAndTilemaps();
    if (IsOverworldLinkActive() || InUnionRoom()) {
      ShowPlayerTrainerCard(CB2_ReturnToFieldWithOpenMenu); // Display trainer card
      DestroyTask(FindTaskIdByFunc(Task_HeatStartMenu_HandleMainInput));
    } else if (FlagGet(FLAG_SYS_FRONTIER_PASS)) {
      ShowFrontierPass(CB2_ReturnToFieldWithOpenMenu); // Display frontier pass
      DestroyTask(FindTaskIdByFunc(Task_HeatStartMenu_HandleMainInput));
    } else {
      ShowPlayerTrainerCard(CB2_ReturnToFieldWithOpenMenu); // Display trainer card
      DestroyTask(FindTaskIdByFunc(Task_HeatStartMenu_HandleMainInput));
    }
  }
}

static u8 RunSaveCallback(void)
{
    // True if text is still printing
    if (RunTextPrintersAndIsPrinter0Active() == TRUE)
    {
        return SAVE_IN_PROGRESS;
    }

    return sSaveDialogCallback();
}

static void SaveStartTimer(void)
{
    sSaveDialogTimer = 60;
}

static bool8 SaveSuccesTimer(void)
{
    sSaveDialogTimer--;


    if (JOY_HELD(A_BUTTON) || JOY_HELD(B_BUTTON))
    {
        PlaySE(SE_SELECT);
        return TRUE;
    }
    if (sSaveDialogTimer == 0)
    {
        return TRUE;
    }

    return FALSE;
}

static bool8 SaveErrorTimer(void)
{
    if (sSaveDialogTimer != 0)
    {
        sSaveDialogTimer--;
    }
    else if (JOY_HELD(A_BUTTON))
    {
        return TRUE;
    }

    return FALSE;
}

static u8 SaveReturnSuccessCallback(void)
{
    if (!IsSEPlaying() && SaveSuccesTimer())
    {
        HideSaveInfoWindow();
        return SAVE_SUCCESS;
    }
    else
    {
        return SAVE_IN_PROGRESS;
    }
}

static u8 SaveSuccessCallback(void)
{
    if (!IsTextPrinterActive(0))
    {
        PlaySE(SE_SAVE);
        sSaveDialogCallback = SaveReturnSuccessCallback;
    }

    return SAVE_IN_PROGRESS;
}

static u8 SaveReturnErrorCallback(void)
{
    if (!SaveErrorTimer())
    {
        return SAVE_IN_PROGRESS;
    }
    else
    {
        HideSaveInfoWindow();
        return SAVE_ERROR;
    }
}

static u8 SaveErrorCallback(void)
{
    if (!IsTextPrinterActive(0))
    {
        PlaySE(SE_BOO);
        sSaveDialogCallback = SaveReturnErrorCallback;
    }

    return SAVE_IN_PROGRESS;
}

static u8 SaveDoSaveCallback(void)
{
    u8 saveStatus;

    IncrementGameStat(GAME_STAT_SAVED_GAME);
    PausePyramidChallenge();

    if (gDifferentSaveFile == TRUE)
    {
        saveStatus = TrySavingData(SAVE_OVERWRITE_DIFFERENT_FILE);
        gDifferentSaveFile = FALSE;
    }
    else
    {
        saveStatus = TrySavingData(SAVE_NORMAL);
    }

    if (saveStatus == SAVE_STATUS_OK)
        ShowSaveMessage(gText_PlayerSavedGame, SaveSuccessCallback);
    else
        ShowSaveMessage(gText_SaveError, SaveErrorCallback);

    SaveStartTimer();
    return SAVE_IN_PROGRESS;
}

static void HideSaveInfoWindow(void) {
  ClearStdWindowAndFrame(sSaveInfoWindowId, FALSE);
  RemoveWindow(sSaveInfoWindowId);
}

static void HideSaveMessageWindow(void) {
  ClearDialogWindowAndFrame(0, TRUE);
}

static u8 SaveOverwriteInputCallback(void)
{
    switch (Menu_ProcessInputNoWrapClearOnChoose())
    {
    case 0: // Yes
        sSaveDialogCallback = SaveSavingMessageCallback;
        return SAVE_IN_PROGRESS;
    case MENU_B_PRESSED:
    case 1: // No
        HideSaveInfoWindow();
        HideSaveMessageWindow();
        return SAVE_CANCELED;
    }

    return SAVE_IN_PROGRESS;
}

static u8 SaveConfirmOverwriteDefaultNoCallback(void)
{
    DisplayYesNoMenuWithDefault(1); // Show Yes/No menu (No selected as default)
    sSaveDialogCallback = SaveOverwriteInputCallback;
    return SAVE_IN_PROGRESS;
}

/*static u8 SaveConfirmOverwriteCallback(void)
{
    DisplayYesNoMenuDefaultYes(); // Show Yes/No menu
    sSaveDialogCallback = SaveOverwriteInputCallback;
    return SAVE_IN_PROGRESS;
}*/

static void ShowSaveMessage(const u8 *message, u8 (*saveCallback)(void)) {
    StringExpandPlaceholders(gStringVar4, message);
    LoadMessageBoxAndFrameGfx(0, TRUE);
    AddTextPrinterForMessage_2(TRUE);
    sSaveDialogCallback = saveCallback;
}

static u8 SaveFileExistsCallback(void)
{
    if (gDifferentSaveFile == TRUE)
    {
        ShowSaveMessage(gText_DifferentSaveFile, SaveConfirmOverwriteDefaultNoCallback);
    }
    else
    {
        sSaveDialogCallback = SaveSavingMessageCallback;
    }

    return SAVE_IN_PROGRESS;
}

static u8 SaveSavingMessageCallback(void) {
  ShowSaveMessage(gText_SavingDontTurnOff, SaveDoSaveCallback);
  return SAVE_IN_PROGRESS;
}

static u8 SaveConfirmInputCallback(void)
{
    switch (Menu_ProcessInputNoWrapClearOnChoose())
    {
    case 0: // Yes
        switch (gSaveFileStatus)
        {
        case SAVE_STATUS_EMPTY:
        case SAVE_STATUS_CORRUPT:
            if (gDifferentSaveFile == FALSE)
            {
                sSaveDialogCallback = SaveFileExistsCallback;
                return SAVE_IN_PROGRESS;
            }

            sSaveDialogCallback = SaveSavingMessageCallback;
            return SAVE_IN_PROGRESS;
        default:
            sSaveDialogCallback = SaveFileExistsCallback;
            return SAVE_IN_PROGRESS;
        }
    case MENU_B_PRESSED: // No break, thats smart 
    case 1: // No
        HideSaveInfoWindow();
        HideSaveMessageWindow();
        return SAVE_CANCELED;
    }

    return SAVE_IN_PROGRESS;
}

static u8 SaveYesNoCallback(void) {
    DisplayYesNoMenuDefaultYes(); // Show Yes/No menu
    sSaveDialogCallback = SaveConfirmInputCallback;
    return SAVE_IN_PROGRESS;
}


static void ShowSaveInfoWindow(void) {
    struct WindowTemplate saveInfoWindow = sSaveInfoWindowTemplate;
    u8 gender;
    u8 color;
    u32 xOffset;
    u32 yOffset;
    //const u8 *suffix;
    //u8 *alignedSuffix = gStringVar3;

    if (!FlagGet(FLAG_SYS_POKEDEX_GET))
    {
        saveInfoWindow.height -= 2;
    }
    
    sSaveInfoWindowId = AddWindow(&saveInfoWindow);
    DrawStdWindowFrame(sSaveInfoWindowId, FALSE);

    gender = gSaveBlock2Ptr->playerGender;
    color = TEXT_COLOR_RED;  // Red when female, blue when male.

    if (gender == MALE)
    {
        color = TEXT_COLOR_BLUE;
    }

    // Print region name
    yOffset = 1;
    BufferSaveMenuText(SAVE_MENU_LOCATION, gStringVar4, TEXT_COLOR_GREEN);
    AddTextPrinterParameterized(sSaveInfoWindowId, FONT_NORMAL, gStringVar4, 0, yOffset, TEXT_SKIP_DRAW, NULL);

    // Print player name
    yOffset += 16;
    AddTextPrinterParameterized(sSaveInfoWindowId, FONT_NORMAL, gText_SavingPlayer, 0, yOffset, TEXT_SKIP_DRAW, NULL);
    BufferSaveMenuText(SAVE_MENU_NAME, gStringVar4, color);
    xOffset = GetStringRightAlignXOffset(FONT_NORMAL, gStringVar4, 0x70);
    PrintPlayerNameOnWindow(sSaveInfoWindowId, gStringVar4, xOffset, yOffset);

    // Print badge count
    yOffset += 16;
    AddTextPrinterParameterized(sSaveInfoWindowId, FONT_NORMAL, gText_SavingBadges, 0, yOffset, TEXT_SKIP_DRAW, NULL);
    BufferSaveMenuText(SAVE_MENU_BADGES, gStringVar4, color);
    xOffset = GetStringRightAlignXOffset(FONT_NORMAL, gStringVar4, 0x70);
    AddTextPrinterParameterized(sSaveInfoWindowId, FONT_NORMAL, gStringVar4, xOffset, yOffset, TEXT_SKIP_DRAW, NULL);

    if (FlagGet(FLAG_SYS_POKEDEX_GET) == TRUE)
    {
        // Print pokedex count
        yOffset += 16;
        AddTextPrinterParameterized(sSaveInfoWindowId, FONT_NORMAL, gText_SavingPokedex, 0, yOffset, TEXT_SKIP_DRAW, NULL);
        BufferSaveMenuText(SAVE_MENU_CAUGHT, gStringVar4, color);
        xOffset = GetStringRightAlignXOffset(FONT_NORMAL, gStringVar4, 0x70);
        AddTextPrinterParameterized(sSaveInfoWindowId, FONT_NORMAL, gStringVar4, xOffset, yOffset, TEXT_SKIP_DRAW, NULL);
    }

    // Print play time
    yOffset += 16;
    AddTextPrinterParameterized(sSaveInfoWindowId, FONT_NORMAL, gText_SavingTime, 0, yOffset, TEXT_SKIP_DRAW, NULL);
    BufferSaveMenuText(SAVE_MENU_PLAY_TIME, gStringVar4, color);
    xOffset = GetStringRightAlignXOffset(FONT_NORMAL, gStringVar4, 0x70);
    AddTextPrinterParameterized(sSaveInfoWindowId, FONT_NORMAL, gStringVar4, xOffset, yOffset, TEXT_SKIP_DRAW, NULL);

    CopyWindowToVram(sSaveInfoWindowId, COPYWIN_GFX);
}

static u8 SaveConfirmSaveCallback(void) {
  ClearStdWindowAndFrame(GetStartMenuWindowId(), FALSE);
  //RemoveStartMenuWindow();
  ShowSaveInfoWindow();

  if (InBattlePyramid()) {
    ShowSaveMessage(gText_BattlePyramidConfirmRest, SaveYesNoCallback);
  } else {
    ShowSaveMessage(gText_ConfirmSave, SaveYesNoCallback);
  }
  return SAVE_IN_PROGRESS;
}

static void InitSave(void)
{
    SaveMapView();
    sSaveDialogCallback = SaveConfirmSaveCallback;
}

static void Task_HandleSave(u8 taskId) {
  switch (RunSaveCallback()) {
    case SAVE_IN_PROGRESS:
      break;
    case SAVE_SUCCESS:
    case SAVE_CANCELED: // Back to start menu
      ClearDialogWindowAndFrameToTransparent(0, TRUE);
      ScriptUnfreezeObjectEvents();  
      UnlockPlayerFieldControls();
      DestroyTask(taskId);
      break;
    case SAVE_ERROR:    // Close start menu
      ClearDialogWindowAndFrameToTransparent(0, TRUE);
      ScriptUnfreezeObjectEvents();
      UnlockPlayerFieldControls();
      SoftResetInBattlePyramid();
      DestroyTask(taskId);
      break;
  }
}

#define STD_WINDOW_BASE_TILE_NUM 0x214
#define STD_WINDOW_PALETTE_NUM 14

static void DoCleanUpAndStartSaveMenu(void) {
  if (!gPaletteFade.active) {
    HeatStartMenu_ExitAndClearTilemap();
    FreezeObjectEvents();
    LoadUserWindowBorderGfx(sSaveInfoWindowId, STD_WINDOW_BASE_TILE_NUM, BG_PLTT_ID(STD_WINDOW_PALETTE_NUM));
    LockPlayerFieldControls();
    DestroyTask(FindTaskIdByFunc(Task_HeatStartMenu_HandleMainInput));
    InitSave();
    CreateTask(Task_HandleSave, 0x80);
  }
}

static void DoCleanUpAndStartSafariZoneRetire(void) {
  if (!gPaletteFade.active) {
    HeatStartMenu_ExitAndClearTilemap();
    FreezeObjectEvents();
    LockPlayerFieldControls();
    DestroyTask(FindTaskIdByFunc(Task_HeatStartMenu_SafariZone_HandleMainInput));
    SafariZoneRetirePrompt();
  }
}
 
static void HeatStartMenu_OpenMenu(void) {
  switch (menuSelected) {
    case MENU_POKETCH:
      DoCleanUpAndChangeCallback(CB2_InitPokeNav);
      break;
    case MENU_POKEDEX:
      DoCleanUpAndChangeCallback(CB2_OpenPokedex);
      break;
    case MENU_PARTY: 
      DoCleanUpAndChangeCallback(CB2_PartyMenuFromStartMenu);
      break;
    case MENU_BAG: 
      DoCleanUpAndChangeCallback(CB2_BagMenuFromStartMenu);
      break;
    case MENU_TRAINER_CARD:
      DoCleanUpAndOpenTrainerCard();
      break;
    case MENU_OPTIONS:
      DoCleanUpAndChangeCallback(CB2_InitOptionMenu);
      break;
  }
}

void GoToHandleInput(void) {
  CreateTask(Task_HeatStartMenu_HandleMainInput, 80);
}

static void HeatStartMenu_HandleInput_DPADDOWN(void)
{
    u8 entries[MAX_ICONS_PER_PAGE];
    u8 count;
    GetVisibleMenuEntriesForPage(entries, &count);

    if (count == 0)
        return;

    u8 oldSelection = menuSelected;

    for (u8 i = 0; i < count; i++) {
        if (entries[i] == menuSelected) {
            u8 newIndex = (i + 1) % count;
            if (entries[newIndex] != oldSelection) {
                menuSelected = entries[newIndex];
                sHeatStartMenu->flag = FLAG_VALUE_NOT_SET;  // reset so animation triggers again
                PlaySE(SE_SELECT);
                HeatStartMenu_UpdateMenuName();
                HeatStartMenu_UpdateAllSpriteAnimations();
            }
            return;
        }
    }

    // fallback
    if (menuSelected != entries[0]) {
        menuSelected = entries[0];
        sHeatStartMenu->flag = FLAG_VALUE_NOT_SET;  // reset here as well
        PlaySE(SE_SELECT);
        HeatStartMenu_UpdateMenuName();
        HeatStartMenu_UpdateAllSpriteAnimations();
    }
}

static void HeatStartMenu_HandleInput_DPADUP(void)
{
    u8 entries[MAX_ICONS_PER_PAGE];
    u8 count;
    GetVisibleMenuEntriesForPage(entries, &count);

    if (count == 0)
        return;

    u8 oldSelection = menuSelected;

    for (u8 i = 0; i < count; i++) {
        if (entries[i] == menuSelected) {
            u8 newIndex = (i + count - 1) % count;
            if (entries[newIndex] != oldSelection) {
                menuSelected = entries[newIndex];
                sHeatStartMenu->flag = FLAG_VALUE_NOT_SET;  // reset so animation triggers again
                PlaySE(SE_SELECT);
                HeatStartMenu_UpdateMenuName();
                HeatStartMenu_UpdateAllSpriteAnimations();
            }
            return;
        }
    }

    // fallback
    if (menuSelected != entries[0]) {
        menuSelected = entries[0];
        sHeatStartMenu->flag = FLAG_VALUE_NOT_SET;  // reset here as well
        PlaySE(SE_SELECT);
        HeatStartMenu_UpdateMenuName();
        HeatStartMenu_UpdateAllSpriteAnimations();
    }
}

static void GetVisibleMenuEntriesForPage(u8 *outEntries, u8 *outCount)
{
    u8 i, count = 0, skipped = 0;
    u8 page = sHeatStartMenu->page;
    u8 start = page * MAX_ICONS_PER_PAGE;

    const u8 menuOrder[] = {
        MENU_POKEDEX,
        MENU_PARTY,
        MENU_BAG,
        MENU_POKETCH,
        MENU_TRAINER_CARD,
        MENU_SAVE,
        MENU_OPTIONS
    };

    for (i = 0; i < ARRAY_COUNT(menuOrder); i++) {
        bool32 visible = FALSE;
        switch (menuOrder[i]) {
            case MENU_POKEDEX:
                visible = FlagGet(FLAG_SYS_POKEDEX_GET);
                break;
            case MENU_PARTY:
                visible = FlagGet(FLAG_SYS_POKEMON_GET);
                break;
            case MENU_POKETCH:
                visible = FlagGet(FLAG_SYS_POKENAV_GET);
                break;
            default:
                visible = TRUE;
                break;
        }

        if (!visible)
            continue;

        if (skipped < start) {
            skipped++;
            continue;
        }

        if (count < MAX_ICONS_PER_PAGE) {
            outEntries[count++] = menuOrder[i];
        } else {
            break;
        }
    }

    *outCount = count;
}

static void Task_HeatStartMenu_HandleMainInput(u8 taskId)
{
    // Safe palette reload if tag is valid
    if (sHeatStartMenu->loadState == 0 && !gPaletteFade.active)
    {
        u32 palIndexIcon = IndexOfSpritePaletteTag(TAG_ICON_PAL);
        u32 palIndexButton = IndexOfSpritePaletteTag(TAG_BUTTON_PAL);

        if (palIndexIcon != 0xFF)
            LoadPalette(sIconPal, OBJ_PLTT_ID(palIndexIcon), PLTT_SIZE_4BPP);

        if (palIndexButton != 0xFF)
            LoadPalette(sButtonPal, OBJ_PLTT_ID(palIndexButton), PLTT_SIZE_4BPP);
    }

    // Confirm selection with A
    if (JOY_NEW(A_BUTTON) && sHeatStartMenu->loadState == 0)
    {
        sLastMenuSelected = menuSelected;
        sLastMenuPage = sHeatStartMenu->page;

        if (menuSelected != MENU_SAVE)
            FadeScreen(FADE_TO_BLACK, 0);

        sHeatStartMenu->loadState = 1;
    }

    // Exit with B
    else if (JOY_NEW(B_BUTTON) && sHeatStartMenu->loadState == 0)
    {
        sLastMenuSelected = menuSelected;
        sLastMenuPage = sHeatStartMenu->page;

        PlaySE(SE_SELECT);
        HeatStartMenu_ExitAndClearTilemap();
        DestroyTask(taskId);
    }

    // Navigation
    else if (gMain.newKeys & DPAD_DOWN && sHeatStartMenu->loadState == 0)
    {
        HeatStartMenu_HandleInput_DPADDOWN();
    }
    else if (gMain.newKeys & DPAD_UP && sHeatStartMenu->loadState == 0)
    {
        HeatStartMenu_HandleInput_DPADUP();
    }

    // Page switch (L/R or DPAD)
    else if ((gMain.newKeys & (R_BUTTON | L_BUTTON | DPAD_RIGHT | DPAD_LEFT))
             && sHeatStartMenu->loadState == 0)
    {
        u32 oldPage = sHeatStartMenu->page;
        u8 oldSelection = menuSelected;

        if ((gMain.newKeys & (R_BUTTON | DPAD_RIGHT)) && sHeatStartMenu->page + 1 < sHeatStartMenu->totalPages)
            sHeatStartMenu->page++;
        else if ((gMain.newKeys & (L_BUTTON | DPAD_LEFT)) && sHeatStartMenu->page > 0)
            sHeatStartMenu->page--;

        if (sHeatStartMenu->page != oldPage)
        {
            HeatStartMenu_DestroySprites();
            HeatStartMenu_DestroyLRButtons();
            FreeSpriteTilesByTag(TAG_ICON_GFX);
            FreeSpritePaletteByTag(TAG_ICON_PAL);

            HeatStartMenu_LoadSprites();
            HeatStartMenu_CreateSprites();
            HeatStartMenu_ShowLRButtons(sHeatStartMenu->page, sHeatStartMenu->totalPages);

            u8 entries[MAX_ICONS_PER_PAGE], count;
            GetVisibleMenuEntriesForPage(entries, &count);

            bool8 found = FALSE;
            for (u8 i = 0; i < count; i++)
            {
                if (entries[i] == oldSelection)
                {
                    menuSelected = oldSelection;
                    found = TRUE;
                    break;
                }
            }

            if (!found && count > 0)
                menuSelected = entries[0];

            HeatStartMenu_UpdateMenuName();
            sHeatStartMenu->flag = 0;
            PlaySE(SE_SELECT);
        }
    }

    // Menu activation
    else if (sHeatStartMenu->loadState == 1)
    {
        if (menuSelected != MENU_SAVE)
            HeatStartMenu_OpenMenu();
        else
            DoCleanUpAndStartSaveMenu();
    }
}

static void HeatStartMenu_SafariZone_HandleInput_DPADDOWN(void) {
  sHeatStartMenu->flag = 0;

  switch (menuSelected) {
    case MENU_OPTIONS:
      menuSelected = MENU_FLAG;
      break;
    default:
      PlaySE(SE_SELECT);
      if (menuSelected == MENU_FLAG) {
        menuSelected = MENU_POKEDEX;
      } else if (menuSelected == MENU_BAG) {
        menuSelected = MENU_TRAINER_CARD;
      } else if (menuSelected == MENU_TRAINER_CARD) {
        menuSelected = MENU_OPTIONS;
      } else {
        menuSelected++;
      }
      break;
  }
  HeatStartMenu_UpdateMenuName();
}

static void HeatStartMenu_SafariZone_HandleInput_DPADUP(void) {
  sHeatStartMenu->flag = 0;

  switch (menuSelected) {
    case MENU_FLAG:
      menuSelected = MENU_OPTIONS;
      break;
    default:
      PlaySE(SE_SELECT);
      if (menuSelected == MENU_POKEDEX) {
        menuSelected = MENU_FLAG;
      } else if (menuSelected == MENU_OPTIONS) {
        menuSelected = MENU_TRAINER_CARD;
      } else if (menuSelected == MENU_TRAINER_CARD) {
        menuSelected = MENU_BAG;
      } else {
        menuSelected--;
      }
      break;
  }
  HeatStartMenu_UpdateMenuName();
}

static void Task_HeatStartMenu_SafariZone_HandleMainInput(u8 taskId) {
  u32 index;
  if (sHeatStartMenu->loadState == 0 && !gPaletteFade.active) {
    index = IndexOfSpritePaletteTag(TAG_ICON_PAL);
    LoadPalette(sIconPal, OBJ_PLTT_ID(index), PLTT_SIZE_4BPP); 
  }

  //HeatStartMenu_UpdateClockDisplay();
  if (JOY_NEW(A_BUTTON)) {
    if (sHeatStartMenu->loadState == 0) {
      if (menuSelected != MENU_FLAG) {
        FadeScreen(FADE_TO_BLACK, 0);
      }
      sHeatStartMenu->loadState = 1;
    }
  } else if (JOY_NEW(B_BUTTON) && sHeatStartMenu->loadState == 0) {
    PlaySE(SE_SELECT);
    HeatStartMenu_ExitAndClearTilemap();  
    DestroyTask(taskId);
  } else if (gMain.newKeys & DPAD_DOWN && sHeatStartMenu->loadState == 0) {
    HeatStartMenu_SafariZone_HandleInput_DPADDOWN();
  } else if (gMain.newKeys & DPAD_UP && sHeatStartMenu->loadState == 0) {
    HeatStartMenu_SafariZone_HandleInput_DPADUP();
  } else if (sHeatStartMenu->loadState == 1) {
    if (menuSelected != MENU_FLAG) {
      HeatStartMenu_OpenMenu();
    } else {
      DoCleanUpAndStartSafariZoneRetire();
    }
  }
}

static u8 GetTotalPageCount(void)
{
    u8 entries[12]; // Max total possible
    u8 count;
    GetVisibleMenuEntriesForPage(entries, &count);
    return (count + MAX_ICONS_PER_PAGE - 1) / MAX_ICONS_PER_PAGE;
}
