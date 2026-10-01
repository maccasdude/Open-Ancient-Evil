// Intro movies, keyboard queue, level file loading, WinMain / window
// procedure / message pump and the generic text menu (0x40d000-0x40e980).
#include "game.h"

static char gIntroPath[0x100];   // 0x4b1760 "x:\\CINEMA\\INTRO.AVI" (drive patched)
static char gIntro2Path[0x100];  // 0x4b1660

// 0x40d000: play the intro movie; a key or click skips it.
int PlayIntroMovie()
{
    StopCDMusic();
    AviPlayer avi;
    if (avi.Open(gIntroPath, &gDDW, gDSound, 0)) {
        avi.SetInterrupt(MovieInterrupt, 1);
        avi.Start();
        do {
            PumpMessages();
        } while (avi.IsPlaying());
        avi.Stop();
    }
    return 1;
}

// 0x40d0e0: movie interrupt check: Escape or a mouse click.
int MovieInterrupt()
{
    uint8_t k[2];
    PumpMessages();
    if (KeyPop(k)) {
        do {
            if (k[0] == 0x1b) return 1;
        } while (KeyPop(k));
    }
    if (MouseLeftClicked()) return 1;
    if (MouseRightClicked()) return 1;
    return 0;
}

// 0x40d130: find the game CD by looking for CINEMA\INTRO.AVI on each CD-ROM
// drive. The port looks in the CD directory (AE_CD_DIR, default "cd" inside
// the game directory). The original refused to start without the CD; the
// port carries on without speech, movies and music instead.
int FindCD()
{
    strcpy(gIntroPath, "x:\\CINEMA\\INTRO.AVI");
    for (char drive = 'D'; drive <= 'Z'; drive++) {
        gIntroPath[0] = drive;
        int fd = w_open(gIntroPath, W_O_RDONLY | W_O_BINARY);
        if (fd > 0) {
            w_close(fd);
            gCDDrive = drive;
            return 1;
        }
        break;   // every drive letter maps to the same CD directory
    }
    fprintf(stderr, "Ancient Evil: CD contents not found in %s (%s); "
            "running without speech, movies and CD music.\n", fileio_cd_dir(), gMsg[276] /* Please click 'OK' then insert your Ancie ... */);
    gIntroPath[0] = 'D';
    gCDDrive = 'D';
    return 1;
}

// 0x40d1e0: second part of the intro.
void PlayIntro2Movie()
{
    AviPlayer avi;
    strcpy(gIntro2Path, "x:\\CINEMA\\intro2.AVI");
    gIntro2Path[0] = gIntroPath[0];
    StopCDMusic();
    ShowMouse(0);
    gDDW.Clear(0);
    gDDW.UpdateScreen();
    if (avi.Open(gIntro2Path, &gDDW, gDSound, 0)) {
        avi.SetPosition(0, 0);
        avi.Start();
        do {
            PumpMessages();
        } while (avi.IsPlaying());
        avi.Stop();
    }
    ShowMouse(1);
}

// ---------------------------------------------------------------------------
// Keyboard queue (0x40d300-0x40d3e0): 20 entries of {character, virtual key}.
// ---------------------------------------------------------------------------

static uint8_t gKeyQueue[20][2];   // 0x4b1870
static int gKeyCount;              // 0x4b1868
static int gKeyHead;               // 0x4b1898
static int gKeyTail;               // 0x4b189c

// 0x40d300
void KeyPush(int ch, unsigned vk)
{
    if (vk > 0x30 && vk < 0x5a) return;   // letters and digits arrive as WM_CHAR
    if (vk > 0x7f || vk == 0x1b) return;
    gKeyQueue[gKeyTail][1] = (uint8_t)vk;
    gKeyQueue[gKeyTail][0] = (uint8_t)ch;
    gKeyTail = (gKeyTail + 1) % 20;
    if (++gKeyCount > 20) {
        gKeyCount = 20;
        gKeyHead = (gKeyHead + 1) % 20;
    }
}

// 0x40d370
int KeyPop(uint8_t *out)
{
    if (!gKeyCount) {
        out[0] = 0;
        out[1] = 0;
        return 0;
    }
    out[0] = gKeyQueue[gKeyHead][0];
    out[1] = gKeyQueue[gKeyHead][1];
    gKeyHead = (gKeyHead + 1) % 20;
    gKeyCount--;
    return 1;
}

// Port: look at the oldest key in the queue without taking it.
bool KeyPeek(uint8_t *out)
{
    if (!gKeyCount) return false;
    out[0] = gKeyQueue[gKeyHead][0];
    out[1] = gKeyQueue[gKeyHead][1];
    return true;
}

// Port: take an Escape out of the queue (the other keys keep their order).
bool KeyTakeEsc()
{
    bool found = false;
    uint8_t keep[20][2];
    int n = 0;
    for (int i = 0; i < gKeyCount; i++) {
        const uint8_t *k = gKeyQueue[(gKeyHead + i) % 20];
        if (!found && k[0] == 0x1b) {
            found = true;
            continue;
        }
        keep[n][0] = k[0];
        keep[n][1] = k[1];
        n++;
    }
    if (!found) return false;
    for (int i = 0; i < n; i++) {
        gKeyQueue[i][0] = keep[i][0];
        gKeyQueue[i][1] = keep[i][1];
    }
    gKeyHead = 0;
    gKeyTail = n % 20;
    gKeyCount = n;
    return true;
}

// 0x40d3c0
void KeyClear()
{
    gKeyTail = 0;
    gKeyHead = 0;
    gKeyCount = 0;
}

// ---------------------------------------------------------------------------
// Level files (0x40d3e0-0x40dc00)
// ---------------------------------------------------------------------------

static uint8_t gLevelHeader[0x3c];   // 0x4b18a0: "AE", w, h, name, title, desc

// 0x40d3e0: read the header of every level and allocate its maps.
void LoadLevelHeaders(const char *dir)
{
    char path[40];
    for (int lvl = 0; lvl < 25; lvl++) {
        sprintf(path, "%sLEVEL%d.LEV", dir, lvl + 1);
        int fd = w_open(path, W_O_RDONLY | W_O_BINARY);
        if (fd < 0) FatalError("WONT OPEN : %s", path);
        w_read(fd, gLevelHeader, 0x3c);
        if (gLevelHeader[0] != 'A' || gLevelHeader[1] != 'E') FatalError("Invalid or corrupt level : %s", path);
        gLevelW[lvl] = gLevelHeader[2];
        gLevelH[lvl] = gLevelHeader[3];
        strcpy(gLevelName[lvl], (char *)gLevelHeader + 4);
        strcpy(gLevelTitle[lvl], (char *)gLevelHeader + 0x14);
        w_close(fd);
        AllocLevel(lvl, gLevelHeader[2], gLevelHeader[3]);
    }
    ClearLevelObjects();
}

// 0x40d530
void ClearLevelObjects()
{
    memset(gFeatures, 0, sizeof(gFeatures));
    memset(gLevelItems, 0, sizeof(gLevelItems));
    memset(gDoors, 0, sizeof(gDoors));
    memset(gLights, 0, sizeof(gLights));
    for (int i = 0; i < 750; i++) gLights[i].f0 = -1;
    for (int i = 0; i < 400; i++) gLevelAux[i].level = -1;
}

// 0x40d5a0
void AllocLevel(int lvl, int w, int h)
{
    int size = w * h;
    gLevelTiles[lvl] = new uint8_t[size];
    if (!gLevelTiles[lvl]) FatalError("No mem for tiles : %d", size);
    gLevelWalls[lvl] = new uint8_t[size];
    gLevelLayout[lvl] = new uint8_t[size];
    gLevelAltWalls[lvl] = new uint8_t[size];
}

// 0x40d640
void FreeLevels()
{
    for (int i = 0; i < 25; i++) {
        delete[] gLevelLayout[i];
        gLevelLayout[i] = nullptr;
        delete[] gLevelWalls[i];
        gLevelWalls[i] = nullptr;
        delete[] gLevelTiles[i];
        gLevelTiles[i] = nullptr;
        delete[] gLevelAltWalls[i];
        gLevelAltWalls[i] = nullptr;
    }
}

// 0x40d6c0
void LoadAllLevels(const char *dir)
{
    char path[80];
    for (int lvl = 0; lvl < 25; lvl++) {
        sprintf(path, "%sLEVEL%d.LEV", dir, lvl + 1);
        LoadLevel(path, lvl);
    }
}

// 0x40d700: read the chunks of one level file.
int LoadLevel(const char *path, int lvl)
{
    int fd = w_open(path, W_O_RDONLY | W_O_BINARY);
    if (fd >= 0) {
        w_read(fd, gLevelHeader, 0x3c);
        if (gLevelHeader[0] != 'A' || gLevelHeader[1] != 'E') return 0;
        strcpy(gLevelDesc[lvl], (char *)gLevelHeader + 0x1c);
        gMapWidth = gLevelHeader[2];
        gMapHeight = gLevelHeader[3];
        int size = gMapHeight * gMapWidth;
        int done = 0;
        for (;;) {
            uint8_t id;
            if (w_read(fd, &id, 1) <= 0) id = 'X';
            uint8_t *arr = nullptr;
            switch (id) {
            case 'A': arr = gLevelAltWalls[lvl]; break;
            case 'L': arr = gLevelLayout[lvl]; break;
            case 'T': arr = gLevelTiles[lvl]; break;
            case 'W': arr = gLevelWalls[lvl]; break;
            case 'D': LoadLevelDoors(fd); break;
            case 'I': LoadLevelItems(fd); break;
            case 'F': LoadLevelFeatures(fd); break;
            case 'l': LoadLevelLights(fd); break;
            case 'a': LoadLevelAux(fd); break;
            case 'X': done = 1; break;
            default:
                plat_message_box("Invalid level chunk", "EEKK!!");
                goto fail;
            }
            if (arr && w_read(fd, arr, size) != size) goto fail;
            if (done) break;
        }
        w_close(fd);
        return 1;
    }
fail:
    fprintf(stderr, "cannot read level!! %s\n", path);
    plat_message_box("Cannot read level!!", path);
    if (fd >= 0) w_close(fd);
    return 0;
}

// 0x40d920
void LoadLevelDoors(int fd)
{
    uint8_t rec[0x12];
    int slot = 0;
    for (;;) {
        w_read(fd, rec, 0x12);
        int16_t w0 = *(int16_t *)rec, w1 = *(int16_t *)(rec + 2);
        if (w0 != 1 || w1 == -1) return;
        if (slot >= 400) continue;
        int i;
        for (i = slot; i < 400; i++)
            if (*(int16_t *)&gDoors[i] == 0) break;
        if (i == 400) continue;
        slot = i;
        memcpy(&gDoors[i], rec, 0x12);
    }
}

// 0x40d9c0
void LoadLevelItems(int fd)
{
    uint8_t rec[0xe];
    int slot = 0;
    for (;;) {
        w_read(fd, rec, 0xe);
        int16_t w0 = *(int16_t *)rec, w2 = *(int16_t *)(rec + 4);
        if (w0 != 1 || w2 == -1) return;
        if (slot >= 1500) continue;
        int i;
        for (i = slot; i < 1500; i++)
            if (gLevelItems[i].active == 0) break;
        if (i == 1500) continue;
        slot = i;
        memcpy(&gLevelItems[i], rec, 0xe);
    }
}

// 0x40da60
void LoadLevelFeatures(int fd)
{
    uint8_t rec[0xa];
    int slot = 0;
    for (;;) {
        w_read(fd, rec, 0xa);
        int16_t w0 = *(int16_t *)rec, w1 = *(int16_t *)(rec + 2);
        if (w0 == 0 || w1 == -1) return;
        if (slot >= 875) continue;
        int i;
        for (i = slot; i < 875; i++)
            if (gFeatures[i].active == 0) break;
        if (i == 875) continue;
        slot = i;
        memcpy(&gFeatures[i], rec, 0xa);
    }
}

// 0x40daf0
void LoadLevelLights(int fd)
{
    uint8_t rec[0x10];
    int slot = 0;
    for (;;) {
        w_read(fd, rec, 0x10);
        if (*(int32_t *)rec == -1) return;
        if (slot >= 750) continue;
        int i;
        for (i = slot; i < 750; i++)
            if (gLights[i].f0 == -1) break;
        if (i == 750) continue;
        slot = i;
        memcpy(&gLights[i], rec, 0x10);
        gNumLights++;
    }
}

// 0x40db80
void LoadLevelAux(int fd)
{
    uint8_t rec[8];
    int slot = 0;
    for (;;) {
        w_read(fd, rec, 8);
        if (*(int16_t *)rec == -1) return;
        if (slot >= 400) continue;
        int i;
        for (i = slot; i < 400; i++)
            if (gLevelAux[i].level == -1) break;
        if (i == 400) continue;
        memcpy(&gLevelAux[i], rec, 8);
        gNumAux++;
        slot = i;
    }
}

// ---------------------------------------------------------------------------
// WinMain, window procedure and message pump (0x40dc00-0x40e030)
// ---------------------------------------------------------------------------

// 0x40dc00
int GameMain()
{
    gHInstance = nullptr;
    plat_set_wndproc(WndProc);
    gRunning = GameInit();
    gAppActive = gRunning;
    while (gRunning) {
        PumpMessages();
        if (gRunning) GameRun();
        gRunning = 0;
        GameShutdown();
    }
    return 0;
}

// 0x40dce0
long WndProc(void *hwnd, unsigned msg, unsigned long wParam, long lParam)
{
    (void)hwnd;
    (void)lParam;
    switch (msg) {
    case WM_DESTROY_:
        plat_request_quit();
        return 0;
    case WM_MOVE_:
        OnWindowMove();
        return 0;
    case WM_SIZE_:
        return 0;
    case WM_ACTIVATEAPP_:
        OnActivateApp((int)wParam);
        return 0;
    case WM_KEYDOWN_:
        KeyPush(0, (unsigned)wParam);
        return 0;
    case WM_CHAR_:
        KeyPush((int)wParam, 0);
        return 0;
    }
    return 0;
}

// 0x40dfb0: handle one pending message (block while the game is inactive),
// then poll the mouse.
void PumpMessages()
{
    for (;;) {
        int r = plat_dispatch_one();
        if (r < 0) {
            // WM_QUIT: the window has gone; the original would have no
            // screen left to draw to. Shut down cleanly.
            GameShutdown();
            exit(0);
        }
        if (r == 0) {
            if (gAppActive) break;
            plat_wait_message();
        }
        // (port: the original handled one message per call; a key press is
        // three messages, so keys queued up behind each other and came
        // through late. Handle all that are waiting.)
    }
    PollMouse();
}

// ---------------------------------------------------------------------------
// Text menus (0x40e4e0-0x40e950)
// ---------------------------------------------------------------------------

static MenuItem *gMenuItems;   // 0x4b8988
static int gMenuCount;         // 0x4b8990

// 0x40e4e0: run a menu; returns the id of the chosen item or -1.
int RunMenu(MenuItem *items, int count, int hiColor, void (*idle)(int, int, int), int flash)
{
    int done = 0;
    int prev = -1;
    gMenuItems = items;
    gMenuCount = count;
    int minL = 10000, minT = 10000;
    for (int i = 0; i < count; i++) {
        if (items[i].left < minL) minL = items[i].left;
        if (items[i].top < minT) minT = items[i].top;
    }
    int maxW = 0, maxH = 0;
    for (int i = 0; i < count; i++) {
        if (items[i].right - minL > maxW) maxW = items[i].right - minL;
        if (items[i].bottom - minT > maxH) maxH = items[i].bottom - minT;
    }
    SetCursor(0);
    ShowMouse(1);
    Sprite *saved = GrabScreen(minL, minT, maxW, maxH);
    gShade.SetShadeLevel(0x1f);
    int sel;
    // Port: the click that opened the menu (its release often comes after
    // the menu is up, e.g. on a pile of items) must not choose an entry:
    // clicks count once the buttons have been let go.
    ResetMouseClicks();
    PollMouse();   // (after the reset: a button still held counts as down)
    bool leftArmed = !MouseButtonDown(0), rightArmed = !MouseButtonDown(1);
    auto LeftClick = [&] {
        bool c = MouseLeftClicked();
        if (!leftArmed) {
            if (!MouseButtonDown(0)) leftArmed = true;
            return false;
        }
        return c;
    };
    auto RightClick = [&] {
        bool c = MouseRightClicked();
        if (!rightArmed) {
            if (!MouseButtonDown(1)) rightArmed = true;
            return false;
        }
        return c;
    };
    for (;;) {
        unsigned t0 = plat_game_ticks();   // GetTickCount
        PumpMessages();
        if (idle) idle(prev, 0, 0);
        sel = -1;
        for (int i = 0; i < count; i++) {
            if (gMouseX >= items[i].left && gMouseX <= items[i].right &&
                gMouseY >= items[i].top && gMouseY <= items[i].bottom)
                sel = i;
        }
        uint8_t key[2];
        if (KeyPop(key)) {
            key[0] = (uint8_t)toupper((signed char)key[0]);
            for (int i = 0; i < count; i++) {
                if ((int8_t)key[0] == items[i].hotkey) {
                    sel = i;
                    done = 1;
                    break;
                }
            }
        }
        if ((LeftClick() || key[0] == 0x0d) && sel != -1) done = 1;
        if (RightClick() || key[0] == 0x1b) {
            sel = -1;
            done = 1;
        }
        saved->Blt(gDDW, minL, minT);
        DrawMenu(sel, hiColor);
        UpdateAndRestore(&gDDW);
        plat_wait_game_ticks(t0, 50);
        if (done) break;
        prev = sel;
    }
    ResetMouseClicks();
    RestoreScreen(minL, minT, saved);
    if (sel == -1) return -1;
    if (flash) {
        MenuItem *it = &items[sel];
        int x = it->x;
        if (x == -1) x = (gDDW.width - gText.StringSize((char *)it->text)) / 2;
        Sprite *spr = nullptr;
        int w = gText.StringSize((char *)it->text);
        if (w) spr = GrabScreen(x, it->y, w, 20);
        DrawMenu(-1, 0);
        for (int n = 3; n; n--) {
            if (spr) spr->Blt(gDDW, x, it->y);
            DrawMenuItem(it->x, it->y, it->text, it->color, it->hiColor);
            if (idle) idle(sel, 0, 0);
            gDDW.UpdateScreen();
            plat_sleep(25);
            if (spr) spr->Blt(gDDW, x, it->y);
            DrawMenuItem(it->x, it->y, it->text, gColorRed, gColorRed);
            if (idle) idle(sel, 0, 0);
            gDDW.UpdateScreen();
            plat_sleep(25);
        }
        if (spr) RestoreScreen(x, it->y, spr);
    }
    return items[sel].id;
}

// 0x40e880
void DrawMenu(int sel, int hiColor)
{
    for (int i = 0; i < gMenuCount; i++) {
        MenuItem *it = &gMenuItems[i];
        if (i == sel)
            DrawMenuItem(it->x, it->y, it->text, hiColor, it->hiColor);
        else
            DrawMenuItem(it->x, it->y, it->text, it->color, it->hiColor);
    }
}

// 0x40e8f0
void DrawMenuItem(int x, int y, const char *text, int color, int hiColor)
{
    if (!text) return;   // (empty entries in some menus)
    gText.SetHighlightColor(hiColor);
    gText.SetColor(color, 0);
    if (x == -1)
        gText.PrintCS(y, (char *)text, 1);
    else
        gText.PrintS(x, y, (char *)text, 1);
}

// 0x40e950: CPUID MMX test.
int HasMMX()
{
    return 1;
}
