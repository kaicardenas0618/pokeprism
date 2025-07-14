#include "global.h"
#include "bg.h"
#include "decompress.h"
#include "dma3.h"
#include "gpu_regs.h"
#include "main.h"
#include "malloc.h"
#include "menu.h"
#include "palette.h"
#include "task.h"
#include "ui_option_menu.h"
#include "constants/rgb.h"

static const u32 sOptionMenuTiles[] = INCBIN_U32("graphics/option_menu/tiles.4bpp.lz");
static const u32 sOptionMenuTilemap[] = INCBIN_U32("graphics/option_menu/tilemap.bin.lz");
static const u32 sOptionMenuPalette[] = INCBIN_U32("graphics/option_menu/palette.gbapal");
static const u32 sOptionMenuBgTilemap[] = INCBIN_U32("graphics/option_menu/scroll_bg.bin.lz");

static void VBlankCB(void);
static void MainCB(void);
static void Task_OptionMenuHandleMainInput(u8 taskId);
static void Task_OptionMenuExitFade(u8 taskId);
static void ExitMenu(u8 taskId);

static u16 *sOptionMenuBgTilemapBuffer;
static u16 *sOptionMenuScrollBgTilemapBuffer;

static const struct BgTemplate sBgTemplates[] =
{
    {
        .bg = 0,
        .charBaseIndex = 0,
        .mapBaseIndex = 31,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 2,
        .baseTile = 0,
    },
    {
        .bg = 1,
        .charBaseIndex = 0,
        .mapBaseIndex = 30,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 3,
        .baseTile = 0,
    },
};

void CB2_InitUIOptionMenu(void)
{
    switch (gMain.state)
    {
    case 0:
        SetVBlankCallback(NULL);
        ResetTasks();
        ResetSpriteData();
        FreeAllSpritePalettes();
        ResetPaletteFade();
        SetGpuReg(REG_OFFSET_DISPCNT, 0);
        ResetBgsAndClearDma3BusyFlags(0);

        sOptionMenuBgTilemapBuffer = Alloc(0x800);
        sOptionMenuScrollBgTilemapBuffer = Alloc(0x800);
        if (!sOptionMenuBgTilemapBuffer || !sOptionMenuScrollBgTilemapBuffer)
        {
            if (sOptionMenuBgTilemapBuffer)
                Free(sOptionMenuBgTilemapBuffer);
            if (sOptionMenuScrollBgTilemapBuffer)
                Free(sOptionMenuScrollBgTilemapBuffer);
            SetMainCallback2(gMain.savedCallback);
            return;
        }
        memset(sOptionMenuBgTilemapBuffer, 0, 0x800);
        memset(sOptionMenuScrollBgTilemapBuffer, 0, 0x800);

        InitBgsFromTemplates(0, sBgTemplates, ARRAY_COUNT(sBgTemplates));
        SetBgTilemapBuffer(0, sOptionMenuBgTilemapBuffer);
        SetBgTilemapBuffer(1, sOptionMenuScrollBgTilemapBuffer);

        ShowBg(0);
        ShowBg(1);

        gMain.state++;
        break;

    case 1:
        DecompressAndCopyTileDataToVram(0, sOptionMenuTiles, 0, 0, 0);
        DecompressDataWithHeaderWram(sOptionMenuTilemap, sOptionMenuBgTilemapBuffer);
        DecompressDataWithHeaderWram(sOptionMenuBgTilemap, sOptionMenuScrollBgTilemapBuffer);
        LoadPalette(sOptionMenuPalette, BG_PLTT_ID(0), sizeof(sOptionMenuPalette));
        CopyBgTilemapBufferToVram(0);
        CopyBgTilemapBufferToVram(1);

        gMain.state++;
        break;
    case 2:
        SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_OBJ_1D_MAP | DISPCNT_OBJ_ON | DISPCNT_BG0_ON | DISPCNT_BG1_ON);
        SetVBlankCallback(VBlankCB);
        SetMainCallback2(MainCB);
        BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
        CreateTask(Task_OptionMenuHandleMainInput, 0);
        break;
    }
}

static void VBlankCB(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();

    if (SCROLLING_BGS)
    {
        ChangeBgX(1, 64, BG_COORD_ADD);
        ChangeBgY(1, 64, BG_COORD_ADD);
    }
}

static void MainCB(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    UpdatePaletteFade();
}

static void Task_OptionMenuHandleMainInput(u8 taskId)
{
    if (gPaletteFade.active)
        return;
        
    if (JOY_NEW(B_BUTTON))
    {
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
        gTasks[taskId].func = Task_OptionMenuExitFade;
        return;
    }
}

static void Task_OptionMenuExitFade(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        ExitMenu(taskId);
    }
}

static void ExitMenu(u8 taskId)
{
    DestroyTask(taskId);
    Free(sOptionMenuBgTilemapBuffer);
    SetMainCallback2(gMain.savedCallback);
}
