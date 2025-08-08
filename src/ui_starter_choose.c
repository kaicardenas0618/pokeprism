#include "global.h"
#include "ui_starter_choose.h"
#include "strings.h"
#include "bg.h"
#include "data.h"
#include "decompress.h"
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
#include "trainer_pokemon_sprites.h"
#include "field_effect.h"
#include "pokedex.h"
#include "script_pokemon_util.h"
#include "pokeball.h"
#include "constants/moves.h"
#include "naming_screen.h"
#include "tv.h"

 /*
    9 Starter Selection Birch Case

    This UI was coded by Archie with Graphics by Mudskip.
 */
 
//==========DEFINES==========//
struct MenuResources
{
    MainCallback savedCallback;     // determines callback to run when we exit. e.g. where do we want to go after closing the menu
    u8 gfxLoadState;
    u16 monSpriteId;
    u16 pokeballSpriteIds[9];
    u16 handSpriteId;
    u16 handPosition;
    u16 selector_x;
    u16 selector_y;
    u16 movingSelector;

};

enum WindowIds
{
    WINDOW_BOTTOM_BAR,
};

enum TextIds
{
    CHOOSE_MON,
    CONFIRM_SELECTION,
    RECIEVED_MON,
};

enum Colors
{
    FONT_BLACK,
    FONT_WHITE,
    FONT_RED,
    FONT_BLUE,
};

static const u8 sMenuWindowFontColors[][3] = 
{
    [FONT_BLACK]  = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_WHITE,      TEXT_COLOR_LIGHT_GRAY},
    [FONT_WHITE]  = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_DARK_GRAY,  TEXT_COLOR_LIGHT_GRAY},
    [FONT_RED]   =  {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_RED,        TEXT_COLOR_LIGHT_GRAY},
    [FONT_BLUE]  =  {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_BLUE,       TEXT_COLOR_LIGHT_GRAY},
};


struct SpriteCordsStruct
{
    u8 x;
    u8 y;
};

enum BallPositions
{
    BALL_TOP_FIRST,
    BALL_TOP_SECOND,
    BALL_TOP_THIRD,
    BALL_TOP_FOURTH,
    BALL_MIDDLE_FIRST,
    BALL_MIDDLE_SECOND,
    BALL_MIDDLE_THIRD,
    BALL_BOTTOM_FIRST,
    BALL_BOTTOM_SECOND,
};

struct MonChoiceData
{ // This is the format used to define a mon, everything left out will default to 0 and be blank or use the in game defaults
    u16 species; // Mon Species ID
    u8 level;   // Mon Level 5
    u16 item;   // Held item, just ITEM_POTION
    u8 ball; // this ballid does not change the design of the ball in the case, only in summary/throwing out to battle 
    u8 nature; // NATURE_JOLLY, NATURE_ETC...
    u8 abilityNum; // this is either 0/1 in vanilla or 0/1/2 in Expansion, its the ability num your mon uses from its possible abilities, not the ability constant itself
    u8 gender; // MON_MALE, MON_FEMALE, MON_GENDERLESS
    u8 evs[6]; // use format {255, 255, 0, 0, 0, 0}
    u8 ivs[6]; // use format {31, 31, 31, 31, 31, 31}
    u16 moves[4]; // use format {MOVE_FIRE_BLAST, MOVE_SHEER_COLD, MOVE_NONE, MOVE_NONE}
    bool8 ggMaxFactor;      // only work in Expansion set to 0 otherwise or leave blank
    u8 teraType;            // only work in Expansion set to 0 otherwise or leave blank
    bool8 isShinyExpansion; // only work in Expansion set to 0 otherwise or leave blank
};

//
//  Making Changes Here Changes The Options In The UI. This is where you define your mons
//
static const struct MonChoiceData sStarterChoices[9] = 
{
    [BALL_TOP_FIRST]        = {SPECIES_NONE, 5},
    [BALL_TOP_SECOND]       = {SPECIES_NONE, 5},
    [BALL_MIDDLE_FIRST]     = {SPECIES_SNIVY, 5},

    [BALL_TOP_THIRD]        = {SPECIES_NONE, 5},
    [BALL_TOP_FOURTH]       = {SPECIES_NONE, 5},
    [BALL_MIDDLE_THIRD]     = {SPECIES_OSHAWOTT, 5},

    [BALL_MIDDLE_SECOND]    = {SPECIES_TEPIG, 5},
    [BALL_BOTTOM_FIRST]     = {SPECIES_NONE, 5},
    [BALL_BOTTOM_SECOND]    = {SPECIES_NONE, 5},
};

//==========EWRAM==========//
static EWRAM_DATA struct MenuResources *sStarterChooseDataPtr = NULL;
static EWRAM_DATA u8 *sBg1TilemapBuffer = NULL;
static EWRAM_DATA u8 *sBg2TilemapBuffer = NULL;

//==========STATIC=DEFINES==========//
static void StarterChooseRunSetup(void);
static bool8 StarterChooseDoGfxSetup(void);
static bool8 StarterChoose_InitBgs(void);
static void StarterChooseFadeAndBail(void);
static bool8 StarterChooseLoadGraphics(void);
static void StarterChoose_InitWindows(void);
static void PrintTextToBottomBar(u8 textId);
static void Task_StarterChooseWaitFadeIn(u8 taskId);
static void Task_StarterChooseMain(u8 taskId);
static void SampleUi_DrawMonIcon(u16 speciesId);
static void Task_DelayedSpriteLoad(u8 taskId);

//==========CONST=DATA==========//
static const struct BgTemplate sMenuBgTemplates[] =
{
    {
        .bg = 0,    // windows, etc
        .charBaseIndex = 0,
        .mapBaseIndex = 31,
        .priority = 0
    }, 
    {
        .bg = 1,    // this bg loads the UI tilemap
        .charBaseIndex = 3,
        .mapBaseIndex = 30,
        .priority = 2
    },
    {
        .bg = 2,    // this bg loads the UI tilemap
        .charBaseIndex = 2,
        .mapBaseIndex = 28,
        .priority = 1
    }
};

static const struct WindowTemplate sMenuWindowTemplates[] = 
{
    [WINDOW_BOTTOM_BAR] = 
    {
        .bg = 0,            // which bg to print text on
        .tilemapLeft = 0,   // position from left (per 8 pixels)
        .tilemapTop = 14,    // position from top (per 8 pixels)
        .width = 30,        // width (per 8 pixels)
        .height = 6,        // height (per 8 pixels)
        .paletteNum = 15,   // palette index to use for text
        .baseBlock = 1,     // tile start in VRAM
    },
    DUMMY_WIN_TEMPLATE
};


//
//  Graphics Pointers to Tilemaps, Tilesets, Spritesheets, Palettes
//
static const u32 sCaseTiles[]   = INCBIN_U32("graphics/starter_choose/ui/case_tiles.4bpp.lz");
static const u32 sCaseTilemap[] = INCBIN_U32("graphics/starter_choose/ui/case_tiles.bin.lz");
static const u16 sCasePalette[] = INCBIN_U16("graphics/starter_choose/ui/case_tiles.gbapal");

static const u32 sTextBgTiles[]   = INCBIN_U32("graphics/starter_choose/ui/text_bg_tiles.4bpp.lz");
static const u32 sTextBgTilemap[] = INCBIN_U32("graphics/starter_choose/ui/text_bg_tiles.bin.lz");
static const u16 sTextBgPalette[] = INCBIN_U16("graphics/starter_choose/ui/text_bg_tiles.gbapal");

static const u32 sPokeballHand_Gfx[] = INCBIN_U32("graphics/starter_choose/ui/pokeball_hand.4bpp.lz");
static const u16 sPokeballHand_Pal[] = INCBIN_U16("graphics/starter_choose/ui/pokeball_hand.gbapal");

//
//  Sprite Data for Pokeball Hand Sprite
//
#define TAG_POKEBALL_CURSOR 20001
static const struct OamData sOamData_PokeballHand =
{
    .size = SPRITE_SIZE(32x32),
    .shape = SPRITE_SHAPE(32x32),
    .priority = 1,
};

static const struct CompressedSpriteSheet sSpriteSheet_PokeballHand =
{
    .data = sPokeballHand_Gfx,
    .size = 32*32*4/2,
    .tag = TAG_POKEBALL_CURSOR,
};

static const struct SpritePalette sSpritePal_PokeballHand =
{
    .data = sPokeballHand_Pal,
    .tag = TAG_POKEBALL_CURSOR
};

static const union AnimCmd sSpriteAnim_PokeballStatic[] =
{
    ANIMCMD_FRAME(0, 32),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd sSpriteAnim_PokeballRockBackAndForth[] =
{
    ANIMCMD_FRAME(0, 16),
    ANIMCMD_FRAME(16, 16),
    ANIMCMD_FRAME(0, 16),
    ANIMCMD_FRAME(32, 16),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd sSpriteAnim_Hand[] =
{
    ANIMCMD_FRAME(48, 32),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd *const sSpriteAnimTable_PokeballHand[] =
{
    sSpriteAnim_PokeballStatic,
    sSpriteAnim_PokeballRockBackAndForth,
    sSpriteAnim_Hand,
};

static const struct SpriteTemplate sSpriteTemplate_PokeballHandMap =
{
    .tileTag = TAG_POKEBALL_CURSOR,
    .paletteTag = TAG_POKEBALL_CURSOR,
    .oam = &sOamData_PokeballHand,
    .anims = sSpriteAnimTable_PokeballHand,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy
};


//
//  This is the Callback for the Hand Cursor that Updates its sprite position when moved by the input control functions
//
#define TOP_ROW_Y 36
#define MIDDLE_ROW_Y 58
#define BOTTOM_ROW_Y 80

static const struct SpriteCordsStruct sBallSpriteCords[3][4] = {
        {{40, TOP_ROW_Y}, {88, TOP_ROW_Y}, {152, TOP_ROW_Y}, {200, TOP_ROW_Y}},
        {{64, MIDDLE_ROW_Y}, {120, MIDDLE_ROW_Y}, {176, MIDDLE_ROW_Y}},
        {{96, BOTTOM_ROW_Y}, {144, BOTTOM_ROW_Y}},
};

static void CursorCallback(struct Sprite *sprite)
{
    struct SpriteCordsStruct current_position = {0,0};
    if(sStarterChooseDataPtr->handPosition <= 3)
        current_position = sBallSpriteCords[0][sStarterChooseDataPtr->handPosition];
    else if(sStarterChooseDataPtr->handPosition <= 6)  
        current_position = sBallSpriteCords[1][sStarterChooseDataPtr->handPosition - 4];
    else
        current_position = sBallSpriteCords[2][sStarterChooseDataPtr->handPosition - 7];

    sprite->x = current_position.x;
    sprite->y = current_position.y - 6;

    if(sStarterChooseDataPtr->movingSelector != TRUE)
    {
        if(sprite->data[5] <= 30)
        {
            sprite->y -= 1;
        }
        else if(sprite->data[5] > 30 && sprite->data[5] < 60)
        {
            sprite->y += 1;
        }
        else
        {
            sprite->y -= 1;
            sprite->data[5] = 0;
        }
        sprite->data[5]++;
    }
    
}

//
//  Create The Hande Cursor Sprite
//
static void CreateHandSprite()
{
    u16 i = 0;
    u16 x, y;
    struct SpriteCordsStruct current_position = {0,0};

    for(i=0; i<9; i++)
    {
        if(sStarterChoices[i].species == SPECIES_NONE) // Choose Non Empty Slot To Start In
            continue;
    
        if(sStarterChooseDataPtr->handPosition <= 3)
        {
            current_position = sBallSpriteCords[0][sStarterChooseDataPtr->handPosition];
            break;
        }
        else if(sStarterChooseDataPtr->handPosition <= 6)  
        {
            current_position = sBallSpriteCords[1][sStarterChooseDataPtr->handPosition - 4];
            break;
        }
        else
        {
            current_position = sBallSpriteCords[2][sStarterChooseDataPtr->handPosition - 7];
            break;
        }
    }

    x = current_position.x;
    y = current_position.y - 6;
    sStarterChooseDataPtr->handPosition = i;
    if (sStarterChooseDataPtr->handSpriteId == SPRITE_NONE)
        sStarterChooseDataPtr->handSpriteId = CreateSpriteAtEnd(&sSpriteTemplate_PokeballHandMap, x, y, 0);
    gSprites[sStarterChooseDataPtr->handSpriteId].invisible = FALSE;
    gSprites[sStarterChooseDataPtr->handSpriteId].callback = CursorCallback;
    StartSpriteAnim(&gSprites[sStarterChooseDataPtr->handSpriteId], 2);
    StartSpriteAnim(&gSprites[sStarterChooseDataPtr->pokeballSpriteIds[sStarterChooseDataPtr->handPosition]], 1);
    SampleUi_DrawMonIcon(sStarterChoices[sStarterChooseDataPtr->handPosition].species);
    
    return;
}

static void DestroyHandSprite()
{
    u8 i = 0;
    for(i = 0; i < 9; i++)
    {
        DestroySprite(&gSprites[sStarterChooseDataPtr->pokeballSpriteIds[i]]);
        sStarterChooseDataPtr->pokeballSpriteIds[i] = SPRITE_NONE;
    }
}

//
//  Create The Pokeball Sprites For Each Slot if Not SPECIES_NONE
//
static void CreatePokeballSprites()
{
    u16 i = 0;

    for(i=0; i<9; i++)
    {
        u16 x, y;
        if(sStarterChoices[i].species == SPECIES_NONE)
            continue;

        if(i <= 3)
        {
            x = sBallSpriteCords[0][i].x;
            y = sBallSpriteCords[0][i].y;
        }
        else if(i <= 6)
        {
            
            x = sBallSpriteCords[1][i - 4].x;
            y = sBallSpriteCords[1][i - 4].y;
        }
        else
        {
            x = sBallSpriteCords[2][i - 7].x;
            y = sBallSpriteCords[2][i - 7].y;
        }
        if (sStarterChooseDataPtr->pokeballSpriteIds[i] == SPRITE_NONE)
            sStarterChooseDataPtr->pokeballSpriteIds[i] = CreateSpriteAtEnd(&sSpriteTemplate_PokeballHandMap, x, y, 1);
        gSprites[sStarterChooseDataPtr->pokeballSpriteIds[i]].invisible = FALSE;
        StartSpriteAnim(&gSprites[sStarterChooseDataPtr->pokeballSpriteIds[i]], 0);

    }   
    return;
}

static void DestroyPokeballSprites()
{
    u8 i = 0;
    for(i = 0; i < 9; i++)
    {
        DestroySprite(&gSprites[sStarterChooseDataPtr->pokeballSpriteIds[i]]);
        sStarterChooseDataPtr->pokeballSpriteIds[i] = SPRITE_NONE;
    }
}


//
//  Draw The Pokemon Sprites
//
#define MON_ICON_X     208
#define MON_ICON_Y     128
#define TAG_MON_SPRITE 30003
static void SampleUi_DrawMonIcon(u16 speciesId)
{
    sStarterChooseDataPtr->monSpriteId = CreateMonPicSprite_Affine(speciesId, 0, 0x8000, TRUE, MON_ICON_X, MON_ICON_Y, 5, TAG_NONE);
    gSprites[sStarterChooseDataPtr->monSpriteId].oam.priority = 0;
}

static void ReloadNewPokemon(u8 taskId) // reload the pokeball after a 4 frame delay to prevent palette problems
{
    gSprites[sStarterChooseDataPtr->monSpriteId].invisible = TRUE;
    FreeResourcesAndDestroySprite(&gSprites[sStarterChooseDataPtr->monSpriteId], sStarterChooseDataPtr->monSpriteId);
    sStarterChooseDataPtr->movingSelector = TRUE;
    gTasks[taskId].func = Task_DelayedSpriteLoad;
    gTasks[taskId].data[11] = 0;
}

static void ChangePositionUpdateSpriteAnims(u16 oldPosition, u8 taskId) // turn off Ball Shaking on old ball and start it on new ball, reload pokemon and text
{
    StartSpriteAnim(&gSprites[sStarterChooseDataPtr->pokeballSpriteIds[oldPosition]], 0);
    StartSpriteAnim(&gSprites[sStarterChooseDataPtr->pokeballSpriteIds[sStarterChooseDataPtr->handPosition]], 1);
    ReloadNewPokemon(taskId);
    PrintTextToBottomBar(CHOOSE_MON);
}

static void StarterChoose_GiveMon() // Function that calls the GiveMon function pulled from Expansion by Lunos and Ghoulslash
{
    u8 *evs = (u8 *) sStarterChoices[sStarterChooseDataPtr->handPosition].evs;
    u8 *ivs = (u8 *) sStarterChoices[sStarterChooseDataPtr->handPosition].ivs;
    u16 *moves = (u16 *) sStarterChoices[sStarterChooseDataPtr->handPosition].moves;
    FlagSet(FLAG_SYS_POKEMON_GET);
    gSpecialVar_Result = StarterChoose_GiveMonParameterized(sStarterChoices[sStarterChooseDataPtr->handPosition].species, sStarterChoices[sStarterChooseDataPtr->handPosition].level, \
                sStarterChoices[sStarterChooseDataPtr->handPosition].item, sStarterChoices[sStarterChooseDataPtr->handPosition].ball, \
                sStarterChoices[sStarterChooseDataPtr->handPosition].nature, sStarterChoices[sStarterChooseDataPtr->handPosition].abilityNum, \
                sStarterChoices[sStarterChooseDataPtr->handPosition].gender, evs, ivs, moves, \
                sStarterChoices[sStarterChooseDataPtr->handPosition].ggMaxFactor, sStarterChoices[sStarterChooseDataPtr->handPosition].teraType,\
                sStarterChoices[sStarterChooseDataPtr->handPosition].isShinyExpansion);
}

//==========FUNCTIONS==========//
// UI loader template functions by Ghoulslash
void Task_OpenStarterChoose(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        CleanupOverworldWindowsAndTilemaps();
        StarterChoose_Init(CB2_ReturnToFieldContinueScriptPlayMapMusic);
        DestroyTask(taskId);
    }
}

// This is our main initialization function if you want to call the menu from elsewhere
void StarterChoose_Init(MainCallback callback)
{
    u16 i = 0;
    if ((sStarterChooseDataPtr = AllocZeroed(sizeof(struct MenuResources))) == NULL)
    {
        SetMainCallback2(callback);
        return;
    }
    
    // initialize stuff
    sStarterChooseDataPtr->gfxLoadState = 0;
    sStarterChooseDataPtr->savedCallback = callback;

    sStarterChooseDataPtr->handSpriteId = SPRITE_NONE;

    for(i=0; i < 9; i++)
    {
        sStarterChooseDataPtr->pokeballSpriteIds[i] = SPRITE_NONE;
    }
    
    SetMainCallback2(StarterChooseRunSetup);
}

static void StarterChooseRunSetup(void)
{
    while (1)
    {
        if (StarterChooseDoGfxSetup() == TRUE)
            break;
    }
}

static void StarterChooseMainCB(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    DoScheduledBgTilemapCopiesToVram();
    UpdatePaletteFade();
}

static void StarterChooseVBlankCB(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

static bool8 StarterChooseDoGfxSetup(void)
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
        if (StarterChoose_InitBgs())
        {
            sStarterChooseDataPtr->gfxLoadState = 0;
            gMain.state++;
        }
        else
        {
            StarterChooseFadeAndBail();
            return TRUE;
        }
        break;
    case 3:
        if (StarterChooseLoadGraphics() == TRUE)
            gMain.state++;
        break;
    case 4:
        LoadMessageBoxAndBorderGfx();
        StarterChoose_InitWindows();
        gMain.state++;
        break;
    case 5:
        CreatePokeballSprites(); // Create Sprites and Print Text
        CreateHandSprite();
        PrintTextToBottomBar(CHOOSE_MON);
        CreateTask(Task_StarterChooseWaitFadeIn, 0);
        BlendPalettes(0xFFFFFFFF, 16, RGB_BLACK);
        gMain.state++;
        break;
    case 6:
        BeginNormalPaletteFade(0xFFFFFFFF, 0, 16, 0, RGB_BLACK);
        gMain.state++;
        break;
    default:
        SetVBlankCallback(StarterChooseVBlankCB);
        SetMainCallback2(StarterChooseMainCB);
        return TRUE;
    }
    return FALSE;
}

#define try_free(ptr) ({        \
    void ** ptr__ = (void **)&(ptr);   \
    if (*ptr__ != NULL)                \
        Free(*ptr__);                  \
})

static void StarterChooseFreeResources(void)
{
    try_free(sStarterChooseDataPtr);
    try_free(sBg1TilemapBuffer);
    try_free(sBg2TilemapBuffer);
    FreeResourcesAndDestroySprite(&gSprites[sStarterChooseDataPtr->monSpriteId], sStarterChooseDataPtr->monSpriteId);
    DestroyPokeballSprites();
    DestroyHandSprite();
    FreeAllWindowBuffers();
}

static void Task_StarterChooseWaitFadeAndBail(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        SetMainCallback2(sStarterChooseDataPtr->savedCallback);
        StarterChooseFreeResources();
        DestroyTask(taskId);
    }
}

static void StarterChooseFadeAndBail(void)
{
    BeginNormalPaletteFade(0xFFFFFFFF, 0, 0, 16, RGB_BLACK);
    CreateTask(Task_StarterChooseWaitFadeAndBail, 0);
    SetVBlankCallback(StarterChooseVBlankCB);
    SetMainCallback2(StarterChooseMainCB);
}

static void Task_StarterChooseWaitFadeIn(u8 taskId)
{
    if (!gPaletteFade.active)
        gTasks[taskId].func = Task_StarterChooseMain;
}

static void Task_StarterChooseTurnOff(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        SetMainCallback2(sStarterChooseDataPtr->savedCallback);
        StarterChooseFreeResources();
        DestroyTask(taskId);
    }
}

static bool8 StarterChoose_InitBgs(void) // Init the bgs and bg tilemap buffers and turn sprites on, also set the bgs to blend
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
    InitBgsFromTemplates(0, sMenuBgTemplates, NELEMS(sMenuBgTemplates));
    SetBgTilemapBuffer(1, sBg1TilemapBuffer);
    SetBgTilemapBuffer(2, sBg2TilemapBuffer);
    ScheduleBgCopyTilemapToVram(1);
    ScheduleBgCopyTilemapToVram(2);
    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP);
    SetGpuReg(REG_OFFSET_BLDCNT, BLDCNT_TGT2_ALL | BLDCNT_EFFECT_BLEND | BLDCNT_TGT1_BG2);
    SetGpuReg(REG_OFFSET_BLDALPHA, BLDALPHA_BLEND(1, 8));
    ShowBg(0);
    ShowBg(1);
    ShowBg(2);
    return TRUE;
}

static bool8 StarterChooseLoadGraphics(void) // load tilesets, tilemaps, spritesheets, and palettes
{
    switch (sStarterChooseDataPtr->gfxLoadState)
    {
    case 0:
        ResetTempTileDataBuffers();
        DecompressAndCopyTileDataToVram(1, sCaseTiles, 0, 0, 0);
        DecompressAndCopyTileDataToVram(2, sTextBgTiles, 0, 0, 0);
        sStarterChooseDataPtr->gfxLoadState++;
        break;
    case 1:
        if (FreeTempTileDataBuffersIfPossible() != TRUE)
        {
            DecompressDataWithHeaderWram(sCaseTilemap, sBg1TilemapBuffer);
            DecompressDataWithHeaderWram(sTextBgTilemap, sBg2TilemapBuffer);
            sStarterChooseDataPtr->gfxLoadState++;
        }
        break;
    case 2:
        LoadCompressedSpriteSheet(&sSpriteSheet_PokeballHand);
        LoadSpritePalette(&sSpritePal_PokeballHand);
        LoadPalette(sCasePalette, 32, 32);
        LoadPalette(sTextBgPalette, 16, 16);
        sStarterChooseDataPtr->gfxLoadState++;
        break;
    default:
        sStarterChooseDataPtr->gfxLoadState = 0;
        return TRUE;
    }
    return FALSE;
}

static void StarterChoose_InitWindows(void)
{
    InitWindows(sMenuWindowTemplates);
    DeactivateAllTextPrinters();
    ScheduleBgCopyTilemapToVram(0);
    
    FillWindowPixelBuffer(WINDOW_BOTTOM_BAR, 0);
    LoadUserWindowBorderGfx(WINDOW_BOTTOM_BAR, 720, 14 * 16);
    PutWindowTilemap(WINDOW_BOTTOM_BAR);
    CopyWindowToVram(WINDOW_BOTTOM_BAR, 3);
    
    ScheduleBgCopyTilemapToVram(2);
}


//
//  Text Printing Function
//
static const u8 sText_ChooseMon[] = _("Choose a Pokémon!");
static const u8 sText_AreYouSure[] = _("Are you sure?    {A_BUTTON} Yes  {B_BUTTON} No");
static const u8 sText_RecievedMon[] = _("Give a Nickname?   {A_BUTTON} Yes  {B_BUTTON} No");
static void PrintTextToBottomBar(u8 textId)
{
    u8 speciesNameArray[16];
    const u8 *mainBarAlternatingText;

    u8 x = 1 + 4;
    u8 y = 1 + 18;

    u16 species = sStarterChoices[sStarterChooseDataPtr->handPosition].species;
    u16 dexNum = SpeciesToNationalPokedexNum(species);    

    FillWindowPixelBuffer(WINDOW_BOTTOM_BAR, PIXEL_FILL(TEXT_COLOR_TRANSPARENT));

    switch(textId)
    {
        case 0:
            mainBarAlternatingText = sText_ChooseMon;
            break;
        case 1:
            mainBarAlternatingText = sText_AreYouSure;
            break;
        case 2:
            mainBarAlternatingText = sText_RecievedMon;
            break;
        default:
            mainBarAlternatingText = sText_ChooseMon;
            break;
    } 
    AddTextPrinterParameterized4(WINDOW_BOTTOM_BAR, FONT_NORMAL, x, y, 0, 0, sMenuWindowFontColors[FONT_WHITE], 0xFF, mainBarAlternatingText);

    if(sStarterChoices[sStarterChooseDataPtr->handPosition].species == SPECIES_NONE)
    {
        PutWindowTilemap(WINDOW_BOTTOM_BAR);
        CopyWindowToVram(WINDOW_BOTTOM_BAR, 3);
        return;
    }
    

    StringCopy(gStringVar1, &gText_NumberClear01[0]);
    ConvertIntToDecimalStringN(gStringVar2, dexNum, STR_CONV_MODE_LEADING_ZEROS, 3);
    StringAppend(gStringVar1, gStringVar2);
    AddTextPrinterParameterized4(WINDOW_BOTTOM_BAR, FONT_NORMAL, x, 1 + 2, 0, 0, sMenuWindowFontColors[FONT_WHITE], 0xFF, gStringVar1);

    AddTextPrinterParameterized4(WINDOW_BOTTOM_BAR, FONT_NORMAL, x + 32, 1 + 2, 0, 0, sMenuWindowFontColors[FONT_WHITE], 0xFF, gText_Dash);

    StringCopy(&speciesNameArray[0], GetSpeciesName(species));
    AddTextPrinterParameterized4(WINDOW_BOTTOM_BAR, FONT_NORMAL, x + 40, 1 + 2, 0, 0, sMenuWindowFontColors[FONT_WHITE], 0xFF, &speciesNameArray[0]);

    PutWindowTilemap(WINDOW_BOTTOM_BAR);
    CopyWindowToVram(WINDOW_BOTTOM_BAR, 3);
}


//
//  Control Flow Tasks for Switching Positions in the Grid and Confirming and Naming a Mon
//
static void Task_DelayedSpriteLoad(u8 taskId) // wait 4 frames after changing the mon you're editing so there are no palette problems
{   
    if (gTasks[taskId].data[11] >= 4)
    {
        if(sStarterChoices[sStarterChooseDataPtr->handPosition].species != SPECIES_NONE)
            SampleUi_DrawMonIcon(sStarterChoices[sStarterChooseDataPtr->handPosition].species);
        gTasks[taskId].func = Task_StarterChooseMain;
        sStarterChooseDataPtr->movingSelector = FALSE;
        return;
    }
    else
    {
        gTasks[taskId].data[11]++;
    }
}

static void Task_WaitForFadeAndOpenNamingScreen(u8 taskId)
{   
    if (!gPaletteFade.active)
    {
        SetMainCallback2(sStarterChooseDataPtr->savedCallback);
        StarterChooseFreeResources();
        DestroyTask(taskId);
        VarSet(VAR_0x8004, gPlayerPartyCount - 1);
        ChangePokemonNickname();
    }
}

static void Task_StarterChooseRecievedMon(u8 taskId)
{
    if(JOY_NEW(A_BUTTON))
    {
        PlaySE(SE_SELECT);
        BeginNormalPaletteFade(0xFFFFFFFF, 0, 0, 16, RGB_BLACK);
        gTasks[taskId].func = Task_WaitForFadeAndOpenNamingScreen;
        return;
    }
    if (JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_SELECT);
        BeginNormalPaletteFade(0xFFFFFFFF, 0, 0, 16, RGB_BLACK);
        gTasks[taskId].func = Task_StarterChooseTurnOff;
        return;
    }
}

static void Task_StarterChooseConfirmSelection(u8 taskId)
{
    if(JOY_NEW(A_BUTTON))
    {
        PlaySE(SE_SELECT);
        PrintTextToBottomBar(RECIEVED_MON);
        StarterChoose_GiveMon();
        gTasks[taskId].func = Task_StarterChooseRecievedMon;
        return;
    }
    if (JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_SELECT);
        PrintTextToBottomBar(CHOOSE_MON);
        gTasks[taskId].func = Task_StarterChooseMain;
        return;
    }
}


/* Main Grid Based Movement Control Flow*/
static void Task_StarterChooseMain(u8 taskId)
{
    u16 oldPosition = sStarterChooseDataPtr->handPosition;
    if(JOY_NEW(DPAD_RIGHT))
    {
        PlaySE(SE_SELECT);
        if(sStarterChooseDataPtr->handPosition <= BALL_TOP_FOURTH) // top row move down
        {
            if(sStarterChooseDataPtr->handPosition == BALL_TOP_FOURTH) // top row move down
                sStarterChooseDataPtr->handPosition = BALL_TOP_FIRST;
            else
                sStarterChooseDataPtr->handPosition += 1;
        }
        else if(sStarterChooseDataPtr->handPosition <= BALL_MIDDLE_THIRD)  // middle row move down
        {
            if(sStarterChooseDataPtr->handPosition == BALL_MIDDLE_THIRD) // top row move down
                sStarterChooseDataPtr->handPosition = BALL_MIDDLE_FIRST;
            else
                sStarterChooseDataPtr->handPosition += 1;
        }
        else  // bottom row move down
        {
            if(sStarterChooseDataPtr->handPosition == BALL_BOTTOM_SECOND) // top row move down
                sStarterChooseDataPtr->handPosition = BALL_BOTTOM_FIRST;
            else
                sStarterChooseDataPtr->handPosition += 1;
        }
        ChangePositionUpdateSpriteAnims(oldPosition, taskId);
        return;
    }
    if(JOY_NEW(DPAD_LEFT))
    {
        PlaySE(SE_SELECT);
        if(sStarterChooseDataPtr->handPosition <= BALL_TOP_FOURTH) // top row move down
        {
            if(sStarterChooseDataPtr->handPosition == BALL_TOP_FIRST) // top row move down
                sStarterChooseDataPtr->handPosition = BALL_TOP_FOURTH;
            else
                sStarterChooseDataPtr->handPosition -= 1;
        }
        else if(sStarterChooseDataPtr->handPosition <= BALL_MIDDLE_THIRD)  // middle row move down
        {
            if(sStarterChooseDataPtr->handPosition == BALL_MIDDLE_FIRST) // top row move down
                sStarterChooseDataPtr->handPosition = BALL_MIDDLE_THIRD;
            else
                sStarterChooseDataPtr->handPosition -= 1;
        }
        else  // bottom row move down
        {
            if(sStarterChooseDataPtr->handPosition == BALL_BOTTOM_FIRST) // top row move down
                sStarterChooseDataPtr->handPosition = BALL_BOTTOM_SECOND;
            else
                sStarterChooseDataPtr->handPosition -= 1;
        }
        ChangePositionUpdateSpriteAnims(oldPosition, taskId);
        return;
    }
    if(JOY_NEW(A_BUTTON))
    {
        if(sStarterChoices[sStarterChooseDataPtr->handPosition].species != SPECIES_NONE) // If spot empty don't go to next control flow state
        {
            PlaySE(SE_SELECT);
            PrintTextToBottomBar(CONFIRM_SELECTION);
            gTasks[taskId].func = Task_StarterChooseConfirmSelection;
            return;
        }
        else
        {
            PlaySE(SE_BOO);
            return;
        }
    }
}
