// Bookshelves and books, warp tiles, breakable walls, level feature editing,
// arrows, the light radius, levers / pressure plates and the trigger system,
// the automap and the mouse-over hints (0x427630-0x429550).
#include "game.h"
#include "../platform/fileio.h"
#include "../platform/platform.h"

// ---- books ------------------------------------------------------------------

enum { kBookPages = 20, kBookLines = 15, kBookLineLen = 50 };

// 0x427630: pick a book from a bookcase.
void ReadBookshelf(int vi)
{
    char flags[52];
    char path[40];
    MenuItem items[10];
    Feature &f = gFeatures[gVisFeatures[vi].feature];
    int x = f.x, y = f.y;
    if (!FeatureInReach(vi)) {
        ShowMessage(gMsg[2] /* It is too far away. */, gColorWhite);
        return;
    }
    if (CountMonsters(flags, 0)) {
        ShowMessage(gMsg[86] /* Monsters in range !! */, gColorWhite);
        return;
    }
    RedrawGameScreen(2);
    gText.SetColor(gColorWhite, 0);
    gText.PrintC(0x94, (char *)gMsg[55] /* You find these books of interest. */);
    gText.PrintC(0xac, (char *)gMsg[57] /* Which will you read ? */);

    uint16_t white = gColorWhite;
    int n = 0;
    for (int i = 0; gBooks[i].level != -1; i++) {
        Book &b = gBooks[i];
        if (b.level != gCurLevel + 1) continue;
        if ((b.x == x && b.y == y) || (b.x2 == x && b.y2 == y)) {
            items[n].text = b.title;
            items[n].color = white;
            items[n].id = i;
            n++;
        }
    }
    items[n].text = gMsg[58] /* None */;
    items[n].color = white;
    items[n].id = -1;
    n++;
    for (int i = 0, yy = 0xc8; i < n; i++, yy += 0x14) {
        MenuItem &m = items[i];
        m.hotkey = -1;      // left uninitialised by the original
        m.hiColor = gColorAzure;
        m.x = -1;
        m.y = yy;
        m.top = yy;
        m.left = 0x5a;
        m.right = 0x212;
        m.bottom = m.top + 0x13;
    }
    ShowMouse(1);
    AddDirtyRect(0, 0, 0x280, 0x1e0);
    int sel = RunMenu(items, n, gColorRed, nullptr, 1);
    if (sel >= 0) {
        sprintf(path, "gamedat\\%s.BOK", gBooks[sel].file);
        ReadBook(path, gBooks[sel].pcx);
    }
    KeyClear();
    gDDW.FillRect(0, 0, 0x27f, 0x1df, 0);
    FreeDirtyRects();
}

// 0x427a90: load a .BOK file into 20 pages of 15 lines. '^' ends the text,
// '|' ends a line, '~' starts a new page; characters are stored +0x1f
// (the book font's glyph offset). Returns the last page index.
static int LoadBookText(const char *path, char **lines)
{
    int fd = w_open(path, W_O_RDONLY | W_O_BINARY);
    if (fd < 0) FatalError("Cannot open : %s", path);
    long len = fd >= 0 ? w_filelength(fd) : 0;
    uint8_t *buf = (uint8_t *)malloc(len + 1);
    if (!buf) FatalError("INSUFFICIENT MEMORY ERROR : %s", path);
    if (fd >= 0) {
        w_read(fd, buf, (unsigned)len);
        w_close(fd);
    }
    buf[len] = '^';     // the original relies on the file ending with '^'
    for (int i = 0; i < kBookPages * kBookLines; i++) {
        lines[i] = new char[kBookLineLen];
        if (!lines[i]) FatalError("MEMORY ERROR : %s", path);
        memset(lines[i], 0, kBookLineLen);
    }
    int line = 0, col = 0, page = 0;
    const uint8_t *p = buf;
    for (;;) {
        uint8_t c = *p++;
        if (c == '^') break;
        if (c == '|') {
            if (page < kBookPages && line < kBookLines && col + 1 < kBookLineLen)
                lines[page * kBookLines + line][col + 1] = 0;
            line++;
            col = 0;
        } else if (c == '~') {
            line = 0;
            col = 0;
            page++;
        } else {
            col++;
            if (page < kBookPages && line < kBookLines && col - 1 < kBookLineLen - 1)
                lines[page * kBookLines + line][col - 1] = (char)(c + 0x1f);
        }
    }
    free(buf);
    return page - 1;
}

// 0x427bd0
static void ShowBookPage(char **lines, int page, int pcxIdx, PCX *pcx)
{
    pcx->Display(0, 0);
    if (pcxIdx >= 0 && pcxIdx <= 1)
        gText.SetColor(gDDW.MakePixel16(0x25, 0x19, 0x10), 0);
    else if (pcxIdx == 2)
        gText.SetColor(gDDW.MakePixel16(0x28, 0x0a, 0x0a), 0);
    char **l = &lines[page * kBookLines];
    for (int y = 0x50; y < 0x17c; y += 0x14) gText.Print(0x96, y, *l++);
    AddDirtyRect(0, 0, 0x280, 0x1e0);
    UpdateAndRestore(&gDDW);
}

// 0x427c80
static void FreeBookText(char **lines)
{
    for (int i = 0; i < kBookPages * kBookLines; i++) {
        delete[] lines[i];
        lines[i] = nullptr;
    }
}

// 0x427890
void ReadBook(const char *path, int pcxIdx)
{
    PCX pcx;
    char name[32];
    char *lines[kBookPages * kBookLines];
    snprintf(name, sizeof name, "gamedat\\%s", gBookPcxNames[pcxIdx]);
    if (!pcx.Init(gDDW, name, 1)) return;
    int last = LoadBookText(path, lines);
    int page = 0, done = 0;
    ShowBookPage(lines, 0, pcxIdx, &pcx);
    ShowMouse(1);
    do {
        PumpMessages();
        uint8_t key[2];
        int prev = 0, next = 0, check = 0;
        if (KeyPop(key)) {
            if (key[0] == 0x1b) done = 1;
            if (key[1] == 0x25) prev = 1;           // left arrow
            else if (key[1] == 0x27) next = 1;      // right arrow
        }
        // The arrow keys jump into the middle of the mouse checks below, so the
        // remaining boxes are still tested against the mouse position.
        if (prev) {
            if (page) ShowBookPage(lines, --page, pcxIdx, &pcx);
            if (MouseInBox(0x22a, 0x14, 0x1e, 0x1e)) next = 1;
            check = 1;
        } else if (next) {
            check = 1;
        } else if (MouseLeftClicked()) {
            if (MouseInBox(0x22a, 0x1a4, 0x1e, 0x1e) && page)
                ShowBookPage(lines, --page, pcxIdx, &pcx);
            if (MouseInBox(0x22a, 0x14, 0x1e, 0x1e)) next = 1;
            check = 1;
        }
        if (check) {
            if (next && page < last) ShowBookPage(lines, ++page, pcxIdx, &pcx);
            if (MouseInBox(0x268, 0x1c8, 0x10, 0x10)) done = 1;
        }
        UpdateAndRestore(&gDDW);
    } while (!done);
    FreeBookText(lines);
    pcx.Release();
}

// ---- warps ------------------------------------------------------------------

// 0x427cb0: step onto a warp tile.
void CheckWarpTile()
{
    int tx = gPlayer.tileX, ty = gPlayer.tileY;
    uint8_t t = gLevelMap[gMapRow[ty] + tx];
    if (t != 2 && t != 3) return;
    if (gPlayer.actionFrame || gPlayer.gameMode == 5) return;
    int w = FindWarp(tx, ty);
    if (w == -1) return;
    Warp(gWarps[w].destX, gWarps[w].destY, t);
}

// 0x427d30: move the player to tile (x,y). kind 2 is a visible teleport
// (flash, sound, camera jumps); otherwise the camera keeps its offset.
void Warp(int x, int y, int kind)
{
    int ox = gPlayer.tileX, oy = gPlayer.tileY;
    ClearFGList();
    if ((uint8_t)kind == 2) {
        gFlashColor = gColorWhite;
        PlaySound(0x37, -1, -1);
        gPlayer.x = (float)((double)x + 0.5);
        gCamX = gPlayer.x;
        gPlayer.y = (float)((double)y + 0.5);
        gCamY = gPlayer.y;
        gPlayer.tileX = (int)gPlayer.x;
        gPlayer.tileY = (int)gPlayer.y;
        gPlayer.gameMode = 0;
        gPlayer.actionFrame = 0;
        gPlayerModel = gPlayerAnim.Loop(0);
    } else {
        double dx = (double)gCamX - gPlayer.x;
        double dy = (double)gCamY - gPlayer.y;
        float fx = (float)x, fy = (float)y;
        gPlayer.x = (float)((double)fx + 0.5);
        gPlayer.y = (float)((double)fy + 0.5);
        gPlayer.tileX = x;
        gPlayer.tileY = y;
        gCamX = (float)(dx + fx + 0.5);
        gCamY = (float)(dy + fy + 0.5);
    }
    gPlayer.height = TileHeight(gPlayer.tileX, gPlayer.tileY);
    g_5c5824 = 1;
    CheckPlayerTile();
    if (gLevelMap[gMapRow[oy] + ox] == 5) ReleasePlate(ox, oy);
}

// 0x427ed0
int FindWarp(int x, int y)
{
    for (int i = 0; gWarps[i].level != -1; i++)
        if (gWarps[i].x == x && gWarps[i].y == y && gWarps[i].level == gCurLevel + 1) return i;
    return -1;
}

// 0x427f20: hitting a cracked wall with a weapon breaks it.
void BreakWall()
{
    int tx = (int)(SinDeg(gPlayer.angle) + gPlayer.x);
    int ty = (int)(gPlayer.y - CosDeg(gPlayer.angle));
    if (gLevelMap[gMapRow[ty] + tx] != 0x14 || !gEquip.weaponType) return;
    for (int i = 0; i < kMaxVisFeatures; i++) {
        int fi = gVisFeatures[i].feature;
        Feature &f = gFeatures[fi];
        if (f.x != tx || f.y != ty) continue;
        if (gFeatureNameIdx[f.sprite] != 0x36) return;
        SetMapCell(tx, ty, 3, 0);
        f.active = 0;
        gVisFeatures[i].active = 0;
        RemoveWall(tx, ty);
        g_5c5824 = 1;
        ShowMessage(gMsg[64] /* The web has been broken */, gColorWhite);
        DropItem(tx, ty, 0x80, 0);
        return;
    }
}

// ---- level features -----------------------------------------------------------

// 0x428040
void RemoveFeature(int level, int x, int y)
{
    for (int i = 0; i < 875; i++) {
        Feature &f = gFeatures[i];
        if (f.active == 1 && f.x == x && f.y == y && f.level == level) {
            f.active = 0;
            SetMapCell(x, y, 3, 0);
            g_5c5824 = 1;
        }
    }
}

// 0x4280a0
void AddFeatureAt(int level, int x, int y, int sprite)
{
    RemoveFeature(level, x, y);
    for (int i = 0; i < 874; i++) {
        Feature &f = gFeatures[i];
        if (f.active) continue;
        f.x = (int16_t)x;
        f.y = (int16_t)y;
        f.level = (int16_t)level;
        f.sprite = (int16_t)sprite;
        f.active = 1;
        FindFeatureRange();
        g_5c5824 = 1;
        return;
    }
}

// 0x428120
void SetFeatureSprite(int x, int y, int sprite)
{
    for (int i = 0; i < 875; i++) {
        Feature &f = gFeatures[i];
        if (f.active == 1 && f.x == x && f.y == y && f.level == gCurLevel) {
            g_5c5824 = 1;
            f.sprite = (int16_t)sprite;
            return;
        }
    }
}

// ---- arrows, dice -------------------------------------------------------------

// 0x428190
int FireArrow()
{
    if (!gPlayer.arrows) {
        ShowMessage(gMsg[65] /* You have no crossbow bolts */, gColorWhite);
        return 0;
    }
    int a = AimAngle();
    float y = gPlayer.y - CosDeg(gPlayer.angle);
    float x = SinDeg(gPlayer.angle) + gPlayer.x;
    if (!FireProjectile(x, y, a, 2, 0)) {
        ShowMessage(gMsg[66] /* Not enough room to fire */, gColorWhite);
        return 0;
    }
    gPlayer.arrows--;
    gPlayer.bowCooldown = (uint8_t)((20 - gStats.dex) / 5);
    return 1;
}

// 0x428250: aim at the monster under the mouse if it is within 60 degrees.
int AimAngle()
{
    int m = MonsterUnderMouse();
    if (m == -1) return gPlayer.angle;
    int a = AngleToMonster(m);
    int d = abs(a - gPlayer.angle);
    if (d > 60 && d < 300) return gPlayer.angle;
    return a;
}

// 0x428290: base + n rolls of lo..hi
int Dice(int base, int lo, int hi, int n)
{
    for (; n > 0; n--) base += rand() % (hi - lo + 1) + lo;
    return base;
}

// 0x4283f0: brightness and radius of the light around the player.
void UpdateLightRadius()
{
    int b = (int)(gTicks / 500) + 8;
    if (b > 12) b = 12;
    int r = b / 2;
    if (gPlayer.gameMode == 6) { b = 16; r = 8; }
    if (gEquip.shield == 2 && gActionMode == 0) { b = 24; r = 10; }  // torch
    if (gLightSpell && b < gLightSpellBright[gLightKind]) {
        r = gLightSpellRadius[gLightKind];
        b = gLightSpellBright[gLightKind];
    }
    if (b != gLightBright) {
        gLightBright = b;
        gLightRadius = r;
        g_5c5824 = 1;
    }
}

// ---- levers, pressure plates, triggers ------------------------------------------

// 0x428490
void PullLeverA(int vi)
{
    int fi = gVisFeatures[vi].feature;
    if (!FeatureInReach(vi)) {
        ShowMessage(gMsg[2] /* It is too far away. */, gColorRed);
        return;
    }
    Feature &f = gFeatures[fi];
    f.sprite++;
    SetTrigger(f.x, f.y, 1);
    g_5c5824 = 1;
    PlaySound(5, -1, -1);
}

// 0x428510
void PullLeverB(int vi)
{
    int fi = gVisFeatures[vi].feature;
    if (!FeatureInReach(vi)) {
        ShowMessage(gMsg[2] /* It is too far away. */, gColorRed);
        return;
    }
    Feature &f = gFeatures[fi];
    f.sprite--;
    SetTrigger(f.x, f.y, 0);
    g_5c5824 = 1;
    PlaySound(5, -1, -1);
}

// 0x428590: release pressure plates nothing is standing on any more.
void CheckPads()
{
    for (int i = 0; gPads[i][0] != -1; i++) {
        int x = gPads[i][0], y = gPads[i][1];
        if (gLevelMap[gMapRow[y] + x] == 5) ReleasePlate(x, y);
    }
}

// 0x4285d0
void PressPlate(int x, int y)
{
    SetMapCell(x, y, 3, 5);
    SetMapCell(x, y, 1, gOverlayLayer[gMapRow[y] + x] + 1);
    g_5c5824 = 1;
    SetTrigger(x, y, 1);
    PlaySound(5, -1, -1);
}

// 0x428630: a plate pops back up unless the player or an item is on it.
void ReleasePlate(int x, int y)
{
    if (gPlayer.tileX == x && gPlayer.tileY == y && gPlayer.gameMode != 5) return;
    for (int i = gItemFirst; i <= gItemLast; i++) {
        LevelItem &it = gLevelItems[i];
        if (it.active == 1 && it.level == gCurLevel + 1 && it.x == x && it.y == y) return;
    }
    SetMapCell(x, y, 3, 4);
    SetMapCell(x, y, 1, gOverlayLayer[gMapRow[y] + x] - 1);
    g_5c5824 = 1;
    SetTrigger(x, y, 0);
    PlaySound(5, -1, -1);
}

// 0x428810
static int AllTriggersSet(int g)
{
    int n = 0;
    for (int i = 0; i < 9; i++) {
        TriggerSwitch &s = gTriggers[g].sw[i];
        if (s.state == s.want || !s.level) n++;
    }
    return n == 9;
}

// 0x428700: a lever or plate at (x,y) changed state.
void SetTrigger(int x, int y, int state)
{
    for (int g = 0; gTriggers[g].param != -1; g++) {
        TriggerGroup &tg = gTriggers[g];
        for (int i = 0; i < 9; i++) {
            TriggerSwitch &s = tg.sw[i];
            if (s.level != gCurLevel + 1 || s.x != x || s.y != y) continue;
            s.state = (uint8_t)state;
            int ok = AllTriggersSet(g);
            gPlayer.noise += 15;
            switch (tg.type) {
            case 1: TriggerWalls(tg.param, ok); break;
            case 2: TriggerDoor(tg.param, ok); break;
            case 3: TriggerFeatures(tg.param, ok); break;
            case 4: TriggerWarp(tg.param, ok); break;
            case 5: TriggerRemoveFeatures(tg.param, ok); break;
            }
        }
    }
}

// 0x428850
void TriggerWalls(int param, int ok)
{
    if (!(uint8_t)ok) return;
    for (int i = 0; i < 6; i++) {
        TrigWall &w = gTrigWalls[param][i];
        if (w.x == -1) continue;
        uint8_t t = (uint8_t)ok == 1 ? w.onTile : w.offTile;
        SetMapCell(w.x, w.y, 3, t);
        SetMapCell(w.x, w.y, 2, 0xff);
        RemoveWall(w.x, w.y);
        ShowMessage(gMsg[268] /* A secret door has opened */, gColorWhite);
    }
}

// 0x4288f0
void TriggerDoor(int param, int ok)
{
    TrigDoor &t = gTrigDoors[param];
    int d = DoorAt(t.x, t.y);
    if (d == -1) return;
    Door &door = gDoors[d];
    if ((uint8_t)ok == 1) {
        if (door.open == 2 || door.lock != 0x14) return;
        door.open = 1;
        door.lock = t.lock;
        PlaySound(gDoorSounds[door.kind], door.x, door.y);
        ShowMessage(gMsg[266] /* A door has opened */, gColorWhite);
    } else {
        if (door.open == 0 || door.lock != 0x14) return;
        door.open = 3;
        PlaySound(gDoorSounds[door.kind], door.x, door.y);
        ShowMessage(gMsg[267] /* A door has closed */, gColorWhite);
    }
}

// 0x428a00
void TriggerFeatures(int param, int ok)
{
    for (int i = 0; i < 6; i++) {
        TrigFeature &f = gTrigFeatures[param][i];
        if (f.x == -1) continue;
        if ((uint8_t)ok == 1) {
            RemoveFeature(gCurLevel, f.x, f.y);
        } else {
            AddFeatureAt(gCurLevel, f.x, f.y, f.sprite);
            SetMapCell(f.x, f.y, 3, 0x14);
        }
    }
}

// 0x428a80: redirect a warp.
void TriggerWarp(int param, int ok)
{
    TrigWarp &t = gTrigWarps[param];
    WarpTile &w = gWarps[FindWarp(t.x, t.y)];
    if ((uint8_t)ok == 1) {
        w.destX = t.onX;
        w.destY = t.onY;
    } else {
        w.destX = t.offX;
        w.destY = t.offY;
    }
}

// 0x428af0
void TriggerRemoveFeatures(int param, int ok)
{
    if (!(uint8_t)ok) return;
    for (int i = 0; i < 6; i++) {
        TrigPos &p = gTrigRemove[param][i];
        if (p.x == -1) continue;
        RemoveFeature(gCurLevel, p.x, p.y);
        RemoveWall(p.x, p.y);
    }
}

// 0x428b40
void FlashScreen(int color)
{
    gFlashColor = 0;
    gDDW.FillRect(0, 0, 0x27f, gViewHeight, (int16_t)color);
}

// ---- automap ------------------------------------------------------------------

// 0x429090
static void DrawMapCell(int x, int y, int t)
{
    int sx = x * 6 + 0x28, sy = (y + 7) * 5;
    uint16_t c;
    if (t >= 0x1c) c = gDDW.MakePixel16(0x80, 0x80, 0x80);
    else switch (t) {
    case 1: c = gDDW.MakePixel16(0x60, 0x60, 0x60); break;
    case 2: c = gDDW.MakePixel16(0xff, 0, 0xff); break;
    case 6: case 7: case 8: c = gDDW.MakePixel16(0, 0, 0xff); break;
    case 10: c = gDDW.MakePixel16(0xff, 0x80, 0x80); break;
    case 13: case 14: c = gDDW.MakePixel16(0x80, 0, 0); break;
    case 20: c = gDDW.MakePixel16(0x70, 0x70, 0x70); break;
    case 26: case 27: c = gDDW.MakePixel16(0x80, 0x40, 0); break;
    default: c = gDDW.MakePixel16(0xc0, 0xc0, 0xc0); break;
    }
    gDDW.TintRect(sx, sy, 6, 5, c);
    if (t == 0x15) gMapSprites[6].Draw(sx, sy, gDDW.surfacePtr, gDDW.pitch);
    else if (t == 0x16) gMapSprites[7].Draw(sx, sy, gDDW.surfacePtr, gDDW.pitch);
}

// 0x428fd0: explored cells of a level.
static void DrawAutoMap(int level)
{
    for (int y = 0; y < gLevelH[level]; y++) {
        for (int x = 0; x < gLevelW[level]; x++) {
            int t = gLevelLayout[level][y * gLevelW[level] + x];
            if (gAutomap[(x / 8 + level * 10) * 65 + y] & gBitMask[x % 8]) DrawMapCell(x, y, t);
        }
    }
}

// 0x428e80
static void DisplayMap(int level, PCX *pcx)
{
    char buf[12];
    LogPrintf(&gLog, "Enter : DisplayMap()\n");
    ShowMouse(0);
    pcx->Display(0, 0, gDDW.MakePixel16(0, 0xff, 0));
    uint8_t *p;
    unsigned long pitch;
    if (!gDDW.Lock(&p, &pitch)) return;
    LogPrintf(&gLog, "calling DrawAutoMap....");
    DrawAutoMap(level);
    LogPrintf(&gLog, "success\n");
    gDDW.Unlock();
    sprintf(buf, "%d", level + 1);
    gText.Print(0x25d, 0x14c, buf, gColorWhite);
    if (level <= gDeepestLevel) gText.Print(0x14, 0x168, gLevelDesc[level], gColorWhite);
    AddDirtyRect(0, 0, 0x280, 0x1e0);
    UpdateAndRestore(&gDDW);
    ShowMouse(1);
    LogPrintf(&gLog, "Exit : DisplayMap()\n");
}

// 0x428b70
void UseMap()
{
    LogPrintf(&gLog, "Enter : UseMap() on level %d\n", gCurLevel + 1);
    int blink = 1, frame = 0, done = 0;
    PCX pcx;
    gUIMode = 8;
    SetCursor(0);
    RedrawGameScreen(3);
    LogPrintf(&gLog, "Backdrop rendered\n");
    pcx.Init(gDDW, (char *)"gamedat\\automap.pcx", 0);
    LogPrintf(&gLog, "Map PCX initialized\n");
    int count = LoadCSpriteTable((char *)"gamedat\\map.cst", gMapSprites, 30);
    if (!count) {
        LogPrintf(&gLog, "Error loading map.cst : %d\n", 0);
        return;
    }
    LogPrintf(&gLog, "CSprite table loaded : %d\n", count);
    gShade.SetShadeLevel(0x1f);
    DisplayMap(gCurLevel, &pcx);
    int level = gCurLevel;
    int mx = gPlayer.tileX * 6 + 0x26;
    int my = gPlayer.tileY * 5 + 0x21;
    ShowMouse(1);
    LogPrintf(&gLog, "Enter message pump\n");
    uint8_t key[2] = {0, 0};
    do {
        if (level == gCurLevel) {
            frame += blink;
            if (frame == 0 || frame == 5) blink = -blink;
            gMapSprites[frame].Draw(mx, my, gDDW);
        }
        if (MouseLeftClicked()) {
            LogPrintf(&gLog, "Left mouse click detected.\n");
            if (MouseInBox(0x26c, 0x15d, 0xf, 0xf)) {
                done++;
            } else if (MouseInBox(0x21d, 0x15d, 0xe, 0xf)) {
                if (level > 0) DisplayMap(--level, &pcx);
            } else if (MouseInBox(0x22b, 0x15d, 0xe, 0xf)) {
                if (level < 0x18) DisplayMap(++level, &pcx);
            }
        }
        KeyPop(key);
        if (key[0] == 0x1b || key[0] == 9 || MouseRightClicked()) done++;
        gDDW.UpdateScreen();
        PumpMessages();
        plat_sleep(50);
    } while (!done);
    LogPrintf(&gLog, "Message pump exited...cleaning up\n");
    KeyClear();
    pcx.Release();
    ReleaseCSpriteTable(gMapSprites, count);
    gDDW.FillRect(0, 0, 0x27f, 0x1df, 0);
    gUIMode = 0;
    LogPrintf(&gLog, "Exit: UseMap() on level %d\n", gCurLevel + 1);
}

// ---- hints --------------------------------------------------------------------

// 0x4294c0: tooltip next to the mouse.
static void DrawHint(const char *text)
{
    int w = gText.StringSize((char *)text);
    int y = gMouseY - 2;
    int x = gMouseX + 0x12;
    if (y > 0x1cc) y = 0x1cc;
    if (x + w > 0x27b) x = gMouseX - w - 10;
    if (x < 5) x = 5;
    gText.SetColor(gColorAzure, 0);
    gText.PrintS(x, y, (char *)text, 1);
}

// 0x4291e0
void DrawHints()
{
    char buf[160];
    if (!gPrefs.hints) return;
    if (gUIMode == 6) {
        if (MouseInBox(0xfa, 0xf5, 0x50, 0x2d)) DrawHint(gMsg[253] /* Open/Close */);
        else if (MouseInBox(0x15e, 0xf5, 0x50, 0x2d)) DrawHint(gMsg[254] /* Search for traps */);
        else if (MouseInBox(0x1c2, 0xf5, 0x50, 0x2d)) DrawHint(gMsg[255] /* Disarm trap */);
        return;
    }
    if (gUIMode != 0 || !gHudVisible) return;
    if (gHintButton == 100) gHintButton = HudButtonAt();
    // Port: the hints name the keys bound now (Options, Controls) instead
    // of the original keys; the other control schemes use the mouse
    // differently, so their hints say so.
    const bool classic = gSettings.scheme == kSchemeClassic;
    switch (gHintButton) {
    case 1: snprintf(buf, sizeof buf, "Drop an item you are carrying. Key : %s", BindText(ACT_DROP)); DrawHint(buf); break;
    case 3: snprintf(buf, sizeof buf, "Use items/objects. Key : %s", BindText(ACT_USE)); DrawHint(buf); break;
    case 2: snprintf(buf, sizeof buf, "Identify items/objects. Key : %s", BindText(ACT_IDENTIFY)); DrawHint(buf); break;
    case 7: snprintf(buf, sizeof buf, "Search for secret doors. Key : %s", BindText(ACT_SEARCH)); DrawHint(buf); break;
    case 4: DrawHint("Options. Key : 'Esc'"); break;
    case 11: snprintf(buf, sizeof buf, "Your health. Key : %s", BindText(ACT_HEALTH)); DrawHint(buf); break;
    case 12: snprintf(buf, sizeof buf, "Your mana. Key : %s", BindText(ACT_HEALTH)); DrawHint(buf); break;
    case 14: snprintf(buf, sizeof buf, "Rune bag - memorize spells. Key : %s", BindText(ACT_MEMORISE)); DrawHint(buf); break;
    case 15: snprintf(buf, sizeof buf, "Display your inventory and statistics. Key : %s", BindText(ACT_INVENTORY)); DrawHint(buf); break;
    case 17:
        if (gActionMode == 0)
            snprintf(buf, sizeof buf, "Action mode is FIGHT - Left click to change (spells: %s).", BindText(ACT_CAST_MODE));
        else if (gActionMode == 1)
            snprintf(buf, sizeof buf, "Action mode is CAST - Left click to change (weapon: %s).", BindText(ACT_ATTACK_MODE));
        else
            snprintf(buf, sizeof buf, "Action mode is JUMP - Left click to change (weapon: %s).", BindText(ACT_ATTACK_MODE));
        DrawHint(buf);
        break;
    case 16:
        if (gActionMode == 1) {
            if (gPlayer.currentSpell != -1)
                snprintf(buf, sizeof buf, "Current spell is %s. Right click to cast.", gSpellNames[gPlayer.currentSpell]);
            else
                snprintf(buf, sizeof buf, "%s", gMsg[195] /* To ready a spell, Left click. */);
            DrawHint(buf);
        } else if (gActionMode == 0) {
            const char *act = classic ? "Right click to strike." : "Left click in the view to strike.";
            if (gEquip.weaponType)
                snprintf(buf, sizeof buf, "Current weapon is %s. %s", gItemNames[gItemTrueNameIdx[gEquipItemId[gEquip.weaponType]]], act);
            else
                snprintf(buf, sizeof buf, "Current weapon is fist. %s", classic ? "Right click to punch." : "Left click in the view to punch.");
            DrawHint(buf);
        }
        break;
    }
}

// 0x429540: the talking mushrooms.
void TalkToMushroom()
{
    RunConversation(&gConvHeaders[3]);
}
