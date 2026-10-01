// Start-up, shut-down, preferences, the main menu and the game loop
// (0x41e480-0x41f9f0).
#include "game.h"
#include "../platform/platform.h"
#include "../platform/fileio.h"

static uint64_t gFrameStart;          // 0x53c430
static uint32_t gLoopStartTime;       // 0x5bc198
static double gFrameSeconds;          // 0x5a67a0
static double gPlayTime;              // 0x5afdd8

static void InitError(const char *msg)
{
    plat_message_box(msg, "Ancient Evil");
}

// 0x41e480
int GameInit()
{
    if (!FindCD()) return 0;
    gLog.Open("rpglog.txt");
    LogPrintf(&gLog, "Start up....\n");
    if (CDAudio_Open(&gCD) != 1)
        fprintf(stderr, "Warning: Error initializing CD Audio\nCD music will not be availible\n");
    LoadPrefs();
    fprintf(stderr, HasMMX() ? "***MMX found!!!\n" : "***Non MMX Machine\n");
    gHwnd = gDDW.Init(gHInstance, (char *)gAppName, 1);
    if (gHwnd) {
        if (gDDW.GetFreeMem() < 0x400 && !(gPrefs.flags & 1))
            fprintf(stderr, "Warning...: %s\n", gMsg[274] /* Possibly insufficient free Video Memory  ... */);
        if (!gDDW.SetModeNumber(0x280, 0x1e0, 0x10)) {
            InitError("No such mode!!");
            return 0;
        }
        if (!gDDW.SetVideoMode()) {
            InitError("Couldn't set mode !!");
            return 0;
        }
        if (!gDDW.CreatePrimarySurface(0)) {
            InitError("Unable to create primary surface(s)");
            return 0;
        }
    }
    OnWindowMove();
    gDDW.Clear(0);
    gClip.SetScreenSize(0x280, 0x1e0);
    gClip.SetViewport(0, 0, 0x27f, 0x1df);
    gViewLeft = 0;
    gViewTop = 0;
    gViewRight = ViewR();   // (0x27f)
    gViewHeight = 0x18f;
    gHudVisible = 1;
    ResetFGList();
    InitFloorCache();
    gShade.Init(0x20, gDDW, gPrefs.gamma);
    SetCSpriteShadeTable(&gShade);
    SetRenderShade(&gShade);
    InitTrigTables();
    LogPrintf(&gLog, "load static gfx\n");
    LoadLevelHeaders("levels\\");
    SoundInit();
    gText.Init(gDDW);
    if (!gFontLarge.Load((char *)"GAMEDAT\\RPG1.CHR")) gDDW.DDError("couldn't load font : RPG1");
    if (!gFontSmall.Load((char *)"GAMEDAT\\AESMALL.CHR")) gDDW.DDError("couldn't load font : AESMALL");
    gText.SetFont(&gFontLarge);
    gText.AntiAlias(1);
    gColorGrey128 = gDDW.MakePixel16(0x80, 0x80, 0x80);
    gColorGrey96 = gDDW.MakePixel16(0x60, 0x60, 0x60);
    gColorRed = gDDW.MakePixel16(0xff, 0, 0);
    gColorNearBlack = gDDW.MakePixel16(1, 1, 1);
    gColorGreen = gDDW.MakePixel16(0, 0xff, 0);
    gColorBlue = gDDW.MakePixel16(0, 0, 0xff);
    gColorAzure = gDDW.MakePixel16(0, 0x80, 0xff);
    gColorWhite = gDDW.MakePixel16(0xff, 0xff, 0xff);
    srand((unsigned)time(nullptr));
    gLevelLoaded = 0;
    StartClock();
    if (!MouseInit(&gDDW, gHwnd)) return 0;
    LoadCursors((char *)"gamedat\\mouse.cst");
    SetCursor(0);
    ShowMouse(0);
    gDDW.RegisterUpdateCallback(MousePreFlip, 1);
    gDDW.RegisterUpdateCallback(MousePostFlip, 2);
    return 1;
}

// 0x41e920
void GameRun()
{
    if (!PlayIntro()) return;
    LoadStaticGfx();
    MainMenuLoop();
}

// 0x41e940
void GameShutdown()
{
    gDDW.Clear(0);
    MouseShutdown();
    StopClock();
    CDAudio_Close(&gCD);
    SoundShutdown();
    gShade.UnInit();
    FreeFloorCache();
    FreeStaticGfx();
    FreeLevels();
    SavePrefs();
    gDDW.End();
    LogPrintf(&gLog, "Game over\n");
    gLog.Close();
}

// 0x41e9c0
void OnWindowMove() { gDDW.OnMove(); }

// 0x41e9d0
void OnActivateApp(int active)
{
    gAppActive = active;
    if (active) {
        StartClock();
        MouseStartTimer();
    } else {
        StopClock();
        MouseStopTimer();
        StopCDMusic();
    }
    gDDW.OnActivate(active);
    gText.Activate(active);
    MouseAcquire(gAppActive);
}

// 0x41ea40
void PollMouse() { MouseRead(&gMouseX, &gMouseY); }

// 0x41ea60
void MainMenuLoop()
{
    LogPrintf(&gLog, "Enter Mainmenu..\n");
    CDAudio_SetVolume(&gCD, gPrefs.cdVolume);
    int r;
    do {
        PumpMessages();
        r = TitleMenu();
        if (r == 0) {
            if (!CharacterGeneration()) continue;
            ShowMouse(0);
            DrawTitleScreen();
            ShowLoadingScreen();
            gDDW.UpdateScreen();
            ResetWorld();
            NewGame();
            ShowMouse(1);
            PlayGame();
        } else if (r == 1) {
            gUIMode = 5;
            r = LoadGameDialog();
            gUIMode = 0;
            if (r) PlayGame();
        }
    } while (r != 2);
    StopCDMusic();
}

static void DefaultPrefs()
{
    gPrefs.cdVolume = 3;
    gPrefs.wavVolume = 5;
    gPrefs.mouseSpeed = 2;
    gPrefs.alwaysRun = 1;
    gPrefs.w0e = 0;
    gPrefs.flags = 0;
    gPrefs.gamma = 4;
    gPrefs.d1c = 0;
    gPrefs.shadows = 1;
    gPrefs.hints = 1;
}

// 0x41eb10
void LoadPrefs()
{
    int fd = w_open("PREFS.CFG", W_O_RDONLY | W_O_BINARY);
    if (fd < 0) {
        DefaultPrefs();
        return;
    }
    if (w_read(fd, &gPrefs, 0x24) != 0x24) DefaultPrefs();
    w_close(fd);
}

// 0x41ec00
void SavePrefs()
{
    gPrefs.flags |= 1;
    int fd = w_open("PREFS.CFG", W_O_WRONLY | W_O_CREAT | W_O_TRUNC | W_O_BINARY, 0600);
    if (fd < 0) {
        FatalError("Error writting PREFS.CFG file", 0);
        return;
    }
    w_write(fd, &gPrefs, 0x24);
    w_close(fd);
}

// 0x41ec60
void LoadStaticGfx()
{
    fprintf(stderr, "loading static GFX\n");
    gBarCount = gHudSprites.Load((char *)"gamedat\\bar.spr", gDDW);
    gActionCount = gActionSprites.Load((char *)"gamedat\\action.spr", gDDW);
    gBloodCount = LoadCSpriteTable((char *)"gamedat\\blood.cst", gDecalSprites, 0x3c);
    gItemSpriteCount = LoadCSpriteTable((char *)"gamedat\\items.cst", gItemSprites, 0x64);
    gItemIconCount = LoadCSpriteTable((char *)"gamedat\\invItems.cst", gItemIcons, 0x64);
    gScreenCount = LoadCSpriteTable((char *)"gamedat\\screen.cst", gScreenSprites, 0x32);
    gSpellFxCount = LoadCSpriteTable((char *)"gamedat\\spellfx.cst", gEffectSprites, 0x78);
    gFaceCount = LoadCSpriteTable((char *)"gamedat\\faces.cst", gFaces, 0x19);
    gTrapSpriteCount = LoadCSpriteTable((char *)"gamedat\\trap.cst", gSparkleSprites, 0xa);
    gAnimSpriteCount = LoadCSpriteTable((char *)"levels\\animtile.cst", gAnimSprites, 0x32);
    gArmsCount = LoadCSpriteTable((char *)"gamedat\\arms.cst", gArmsSprites, 0x32);
    gCharPCX.Init(gDDW, (char *)"GAMEDAT\\CHAR.PCX", 1);
    gRunesPCX.Init(gDDW, (char *)"gamedat\\spell.pcx", 1);
    LoadServantModels();
    LoadProjectileModels();
    LoadRatModel();
}

// 0x41eda0
void FreeStaticGfx()
{
    FreeServantModels();
    FreeRatModel();
    FreeProjectileModels();
    ReleaseCSpriteTable(gFaces, gFaceCount);
    ReleaseCSpriteTable(gEffectSprites, gSpellFxCount);
    ReleaseCSpriteTable(gDoorSprites, gDoorSpriteCount);
    ReleaseCSpriteTable(gDecalSprites, gBloodCount);
    ReleaseCSpriteTable(gFeatureSprites, gFeatureSpriteCount);
    ReleaseCSpriteTable(gObjSprites, gObjSpriteCount);
    ReleaseCSpriteTable(gArmsSprites, gArmsCount);
    ReleaseCSpriteTable(gItemSprites, gItemSpriteCount);
    ReleaseCSpriteTable(gItemIcons, gItemIconCount);
    ReleaseCSpriteTable(gScreenSprites, gScreenCount);
    FreeFloorTiles();
    gHudSprites.Unload();
    gActionSprites.Unload();
    gCharPCX.Release();
    gRunesPCX.Release();
}

// 0x41ee90
// Port (enhanced mode): full-screen menus sit in the middle of the 16:9 screen,
// the game view keeps the status bar on the bottom edge.
void SetScreenOriginY(int oy)
{
    if (!gLayout.wide() || gLayout.oy == oy) return;
    uint8_t *p;
    unsigned long pitch;
    const bool locked = gDDW.LockFront(&p, &pitch) != 0;   // (the cursor thread must not draw meanwhile)
    gDDW.Clear(0);
    gLayout.oy = oy;
    gDDW.Clear(0);
    MouseUpdateRange();
    if (locked) gDDW.UnlockFront();
    gDDW.Lock(&p, &pitch);   // (point surfacePtr back at the back buffer)
    gDDW.Unlock();
}

void PlayGame()
{
    SetScreenOriginY(gLayout.physH - 480);
    EnterLevel(gCurLevel);
    SetCursor(0);
    SetWaveVolume(gPrefs.wavVolume);
    int r = GameLoop();
    SetCursor(0);
    ShowMouse(0);
    LeaveGame();
    SetScreenOriginY((gLayout.physH - 480) / 2);
    if (r == 7) PlayIntro2Movie();
}

// 0x41eef0: copy the initial world state out of the executable's data.
void ResetWorld()
{
    memcpy(gDecals, gDecalsInit, 0x834 * 4);
    memcpy(gLevelObjs, gLevelObjsInit, 0x7e0 * 4);
    memcpy(gContainers, gContainersInit, 0x1d4 * 4);
    ClearLevelObjects();
    LoadAllLevels("LEVELS\\");
}

// 0x41ef40: fresh game state for a new character.
void NewGame()
{
    gCurLevel = 0;
    ClearWounds();
    gViewLeft = 0;
    gViewTop = 0;
    gViewRight = ViewR();   // (0x27f)
    gViewHeight = 0x18f;
    gHudVisible = 1;
    gPlayer.x = 2.5f;
    gCamX = 2.5f;
    gPlayer.y = 19.5f;
    gCamY = 19.5f;
    UpdateLightRadius();
    gInvScroll = 0;
    gActionMode = 0;
    gFlashColor = 0;
    gTicks = 0;
    gFrameCounter = 0;
    gPlayer.angle = 0x5a;
    gPlayer.gameMode = 0;
    gPlayer.pendingAction = -1;
    gUIMode = 0;
    gFGCount = 0;
    gPlayer.bowCooldown = 0;
    gPlayer.attacking = 0;
    gPlayer.height = 0;
    gPlayer.actionFrame = 0;
    gPlayer.speed = gWalkSpeed;
    gPlayer.currentSpell = -1;
    gPlayer.castSpell = -1;
    gTorchFuel = 0;
    g_5be7c4 = 0;
    gTimeStop = 0;
    gStatuesDestroyed = 0;
    g_5bc194 = 0;
    ClearSpellHotkeys();
    // conversation flags 0x5a6590..0x5a659e
    g_5a6590 = 0;
    gMetJetraal = 0;
    g_5a6593 = 0;
    gTalkedToPriest = 0;
    gMetGremlins = 0;
    gGremlinsGotOrb = 0;
    g_5a6598 = 0;
    g_5a659c = 0;
    g_5a659e = 0;
    memset(gProjectiles, 0, 0x8c * 4);
    ClearServants();
    gLightSpell = 0;
    gLightKind = 0;
    gLightFrame = 0;
    gLightSx = 0;
    gLightSy = 0;
    gLocFxActive = 0;
    gLocFxKind = 0;
    gLocFxX = 0;
    gLocFxY = 0;
    gLocFxSprite = 0;
    gLocFxDepth = 0;
    gLocFxFrame = 0;
    gLocFxUnused = 0;
    gTrapActive = 0; gTrapFrame = 0; gTrapX = gTrapY = 0; gTrapSx = gTrapSy = 0;
    memset(gBurners, 0, sizeof(gBurners));
    memset(gAutomap, 0, 0xfde * 4 + 2);
    InitAllMonsters();
    memcpy(gWarps, gInit44d008, sizeof(gWarps));
    memcpy(gShopItems, gShopItemsInit, sizeof(gShopItems));
    memcpy(gTriggers, gInit44dfd8, sizeof(gTriggers));
    memcpy(gContainers, gContainersInit, sizeof(gContainersInit));
    SortLevelItems();
    memcpy(gTraps, gTrapsInit, sizeof(gTrapsInit));
    ResetConversations();
    RecalcStats();
    UpdateWalkSpeed();
    LoadPlayerTextures();
    // Port debugging aid (not in the original): AE_DEBUG_LEVEL=n starts a
    // new game at the arrival point of the first stairs into level n (1-25).
    if (const char *dl = getenv("AE_DEBUG_LEVEL")) {
        int want = atoi(dl) - 1;
        for (int i = 0; want > 0 && gLevelLinks[i].level != -1; i++) {
            LevelLink &l = gLevelLinks[i];
            if (l.destLevel != want) continue;
            gCurLevel = want;
            gPlayer.x = gCamX = (float)l.destX + 0.5f;
            gPlayer.y = gCamY = (float)l.destY + 0.5f;
            break;
        }
    }
    // AE_DEBUG_POS=x,y puts the player on that tile (debugging aid)
    if (const char *dp = getenv("AE_DEBUG_POS")) {
        int x = 0, y = 0;
        if (sscanf(dp, "%d,%d", &x, &y) == 2) {
            gPlayer.x = gCamX = (float)x + 0.5f;
            gPlayer.y = gCamY = (float)y + 0.5f;
        }
    }
}

// Port: AE_TIMING=1 reports the real frame and game clock rates.
static void TimingProbe()
{
    static int on = -1;
    if (on < 0) on = getenv("AE_TIMING") != nullptr;
    if (!on) return;
    static uint64_t t0, last;
    static int frames, time0;
    static double worst, sumsq;
    uint64_t now = plat_perf_counter();
    double f = (double)plat_perf_freq();
    if (!t0) {
        t0 = last = now;
        time0 = gTime;
        return;
    }
    double dt = (double)(now - last) * 1000.0 / f;
    last = now;
    frames++;
    sumsq += dt * dt;
    if (dt > worst) worst = dt;
    double el = (double)(now - t0) / f;
    if (el >= 5.0) {
        double mean = el * 1000.0 / frames;
        double sd = sqrt(sumsq / frames - mean * mean);
        fprintf(stderr, "[timing] frame %.3f ms (%.3f fps) jitter %.2f ms worst %.1f ms, clock %.3f Hz\n", mean,
                frames / el, sd, worst, (gTime - time0) / el);
        t0 = now;
        time0 = gTime;
        frames = 0;
        worst = sumsq = 0;
    }
}



// 0x41f130: one call plays the game until death, quitting or the end.
int GameLoop()
{
    fprintf(stderr, "enter game loop\n");
    gLoopStartTime = (uint32_t)gTime;
    g_5c5824 = 1;
    for (;;) {
        gFrameStart = plat_perf_counter();
        PumpMessages();
        InputFrameBegin();
        unsigned t0 = plat_game_ticks();   // GetTickCount
        gHintButton = 100;
        if (ProcessKeys() == 2) return 2;
        if (ProcessMouse() == 2) return 2;
        UpdatePlayer();
        CheckStandingTile();
        CheckPads();
        CheckWarpTile();
        if (!(gFrameCounter & 1)) {
            UpdateLighting();
            g_5c5824 = 1;
        }
        if (g_5c5824) ComputeView(gPlayer.tileX, gPlayer.tileY, 0);
        gDDW.FillRect((RECT *)nullptr, 0);
        if (!gFlashColor) RenderView();
        DrawLevelObjs();
        UpdateMonsters();
        MonstersCheckTraps();
        UpdateProjectiles();
        UpdateServants();
        UpdateLocationEffect();
        if (CheckPit()) return 0xe;
        RingEffects();
        UpdateRats();
        UpdateDoors();
        UpdateBurners();
        UpdateTrap();
        UpdateParticles();
        UpdateBolts();
        if (!gFlashColor) {
            DrawPlayer();
            DrawForceField();
            DrawMonsters();
            DrawServants();
            DrawProjectiles();
            DrawParticles();
            DrawBolts();
            DrawSparkles();
            DrawFGObjects();
        } else {
            FlashScreen(gFlashColor);
        }
        DrawHud();
        ClearWallFlags();
        UpdateTimers();
        DrawEffectIcons();
        ExpireMessages();
        DrawMessages();
        DrawHints();
        UpdateCursor();
        if (gUIMode == 2) DrawInfoText(0);
        // frame limiter: at least 49 ms of GetTickCount per frame
        plat_wait_game_ticks(t0, 0x31);
        gDDW.UpdateScreen();
        TimingProbe();
        UpdateTickRate();
        if (CheckPlayerDeath() == 0xd) return 0xd;
        if (CheckStoryEvents() == 7) return 7;
        if (!gTimeStop) gTicks++;
        gFrameCounter++;
        UpdateCDMusic();
        int elapsed = gTime - (int)gLoopStartTime;
        gLoopFrames++;
        gPlayTime = (double)elapsed * 0.04f;
    }
}

// 0x41f380: frames per second of the last frame.
void UpdateTickRate()
{
    uint64_t now = plat_perf_counter();
    uint64_t freq = plat_perf_freq();
    double elapsed = (double)now - (double)gFrameStart;
    gFrameSeconds = elapsed / (double)freq;
    gTickRate = 1000.0 / gFrameSeconds * 0.001;
}

// 0x41f420
void UpdateCursor()
{
    switch (gUIMode) {
    case 1: SetCursor(3); return;
    case 2: SetCursor(1); return;
    }
    if (gBusyCursor) {
        gBusyCursor--;
        SetCursor(5);
        return;
    }
    if (ActionAny(ACT_FACE)) {
        SetCursor(0);
        return;
    }
    if (gItemHover) {
        SetCursor(3);
        return;
    }
    if (gDoorHover != -1 || gFeatureHover != -1) {
        SetCursor(4);
        return;
    }
    SetCursor(0);
}

// 0x41f4b0: next key press (upper-cased) into gKeyBuf.
int ReadKey()
{
    if (gPlayer.gameMode) return 0;
    if (!KeyPop(gKeyBuf)) {
        gKeyBuf[0] = 0;
        gKeyBuf[1] = 0;
        return 0;
    }
    TranslateGameKey(gKeyBuf);   // port: key bindings (identity for the classic defaults)
    gKeyBuf[0] = (uint8_t)toupper((char)gKeyBuf[0]);
    return 1;
}

static void SelectAction(int mode)
{
    gPrevActionMode = gActionMode;
    gActionMode = mode;
    gPlayer.actionFrame = 0;
    gPlayer.gameMode = 0x13;
}

static void EnterItemMode(int mode, int cursor)
{
    gUIMode = mode;
    gSavedHudVisible = gHudVisible;
    gHudVisible = 1;
    gViewHeight = 0x18f;
    SetCursor(cursor);
    CompactInventory();
}

static void GammaMessage()
{
    char buf[40];
    gShade.SetGammaLevel((float)gPrefs.gamma);
    ClearFloorCache();
    sprintf(buf, "%s%d", gMsg[222] /* Gamma correction level : */, gPrefs.gamma);
    ShowMessage(buf, gColorWhite);
}

// 0x41f500: keyboard commands.
int ProcessKeys()
{
    if (ActionHeld(ACT_ATTACK_HOLD) && gActionMode == 0) gAttackRequest++;
    // Port: the original reads no keys at all until the player stands
    // still, so Escape did nothing while walking (much of the time with the
    // Diablo and WASD schemes) and then opened the menu late. Escape now
    // stops a walk and opens the menu at once, or as soon as an attack,
    // spell or jump has finished.
    static bool escPending;
    if (gUIMode == 0 && KeyTakeEsc()) {
        escPending = true;
        if (gPlayer.gameMode == 2) PlayerStop();
    }
    if (escPending && gUIMode != 0) escPending = false;
    if (escPending && !gPlayer.gameMode) {
        escPending = false;
        return OptionsMenu();
    }
    // (likewise a command key stops a walk and is acted on now, instead of
    // waiting in the queue until the player stops; other keys are dropped
    // meanwhile, as reading them would do)
    if (gPlayer.gameMode == 2 && gUIMode == 0) {
        uint8_t k[2];
        while (KeyPeek(k) && !KeyIsCommand(k[0], k[1])) KeyPop(k);
        if (KeyPeek(k)) PlayerStop();
    }
    if (gPlayer.gameMode || !ReadKey()) return 0;
    do {
        uint8_t ch = gKeyBuf[0], vk = gKeyBuf[1];
        if (ch == '=' && gLightBright < 0x1f) {
            gLightRadius = 0xf;
            gLightBright += 2;
        }
        if (ch == '-' && gLightBright > 0) gLightBright--;
        switch (vk) {
        case 0x08:     // backspace: toggle the status bar
            gViewHeight = 0x1df;
            gHudVisible = ~gHudVisible & 1;
            if (gHudVisible) gViewHeight = 0x18f;
            break;
        case 0x78:     // F9
            MouseSpeedDialog();
            break;
        case 0x7a:     // F11: darker
            if (--gPrefs.gamma < 0) gPrefs.gamma = 0;
            else GammaMessage();
            break;
        case 0x7b:     // F12: brighter
            if (++gPrefs.gamma > 8) gPrefs.gamma = 8;
            else GammaMessage();
            break;
        }
        ch = gKeyBuf[0];
        int m = gUIMode;
        if (m == 1 || m == 3 || m == 2) return 0;
        if (m == 0) {
            if (ch >= '0' && ch <= '9') {
                UseSpellHotkey((char)ch);
                ch = gKeyBuf[0];
                m = gUIMode;
            }
            switch (ch) {
            case '.': case '>':
                gCamX = gPlayer.x;
                gCamY = gPlayer.y;
                ShowMouse(1);
                g_5c5824 = 1;
                break;
            case 'J': SelectAction(2); break;
            case 'A': SelectAction(0); break;
            case 'C': SelectAction(1); break;
            case 'M': MemorizeDialog(); break;
            case 'H':
                ShowHealth();
                ShowMana();
                break;
            case 'O': OpenNearbyDoors(); break;
            case 'E': UseElixir(); break;
            case 'D': EnterItemMode(1, 3); break;
            case 'U': EnterItemMode(3, 4); break;
            case 'I':
                ShowMessage(gMsg[13] /* Identify what ? */, gColorWhite);
                SetCursor(1);
                CompactInventory();
                gUIMode = 2;
                break;
            case 'S': SearchSecrets(); break;
            case ' ': InventoryScreen(); break;
            case '\t':
                if (gHaveMap) {
                    LogPrintf(&gLog, "TAB pressed...calling UseMap()\n");
                    UseMap();
                }
                break;
            case 'K': HotkeyDialog(); break;
            case 0x1b:
                if (m == 1 || m == 3) return 0;
                return OptionsMenu();
            }
        }
    } while (ReadKey());
    return 0;
}
