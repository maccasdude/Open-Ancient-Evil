// Save and load games (0x42ab40-0x42b920).
//
// SAVEn.AE: a 0x40 byte SaveHeader, 29 data blocks XORed with the header's
// key, then the four map layers of all 25 levels. The original wrote its
// global arrays straight from memory; here every block is converted to and
// from the original 32-bit layout, so save files stay compatible with the
// Windows version. Pointers into static tables are written as the addresses
// the original would have stored; pointers to loaded models are written as 0
// and rebuilt after loading (the original relinks them the same way).
#include "game.h"
#include "../platform/fileio.h"

// ---- original-layout conversion -------------------------------------------------

namespace {

struct Xfer {
    bool save;
    uint8_t *buf;
    template <typename T> void f(int off, T &v)
    {
        if (save) memcpy(buf + off, &v, sizeof(T));
        else memcpy(&v, buf + off, sizeof(T));
    }
    void raw(int off, void *p, int n)
    {
        if (save) memcpy(buf + off, p, n);
        else memcpy(p, buf + off, n);
    }
    // pointer to element of a static array: stored as the original address
    template <typename T> void ptr(int off, T *&p, T *base, int count, uint32_t origBase, int origSize)
    {
        uint32_t v = 0;
        if (save) {
            if (p && p >= base && p < base + count) v = origBase + (uint32_t)(p - base) * origSize;
            memcpy(buf + off, &v, 4);
        } else {
            memcpy(&v, buf + off, 4);
            if (v >= origBase && v < origBase + (uint32_t)count * origSize && (v - origBase) % origSize == 0)
                p = base + (v - origBase) / origSize;
            else
                p = nullptr;
        }
    }
    // pointer to loaded data: not meaningful in a file
    template <typename T> void heap(int off, T *&p)
    {
        if (save) memset(buf + off, 0, 4);
        else p = nullptr;
    }
};

const uint32_t kOrigMonsters = 0x4609a8, kOrigStats = 0x43d888, kOrigSpawns = 0x43f600;
const uint32_t kOrigServants = 0x5b91d0, kOrigConvNodes = 0x45e4f0;

// AnimSet (0x3c); 'owner' is the original address of the Model* it updates.
void XAnim(Xfer &x, int o, AnimSet &a, Model **ownerModel, uint32_t ownerOrig)
{
    x.f(o + 0x00, a.f00);
    x.f(o + 0x04, a.count);
    x.heap(o + 0x08, a.models);
    x.f(o + 0x0c, a.f0c);
    x.f(o + 0x10, a.f10);
    x.f(o + 0x14, a.curIndex);
    x.heap(o + 0x18, a.cur);
    x.f(o + 0x1c, a.defaultIndex);
    x.f(o + 0x20, a.frame);
    x.f(o + 0x24, a.numFrames);
    x.f(o + 0x28, a.f28);
    x.f(o + 0x2c, a.f2c);
    x.f(o + 0x30, a.mode);
    uint32_t v = 0;
    if (x.save) {
        if (a.outModel == ownerModel) v = ownerOrig;
        memcpy(x.buf + o + 0x34, &v, 4);
    } else {
        memcpy(&v, x.buf + o + 0x34, 4);
        a.outModel = v ? ownerModel : nullptr;
    }
    x.f(o + 0x38, a.nextIndex);
}

void XMonster(Xfer &x, int o, Monster &m, int idx)
{
    uint32_t orig = kOrigMonsters + idx * 0xe0;
    x.f(o + 0x00, m.x);
    x.f(o + 0x04, m.y);
    x.f(o + 0x08, m.tileX);
    x.f(o + 0x0c, m.tileY);
    x.f(o + 0x10, m.state);
    x.f(o + 0x11, m.type);
    x.raw(o + 0x12, m.pad12, 2);
    x.f(o + 0x14, m.radius);
    x.f(o + 0x18, m.attitude);
    x.f(o + 0x19, m.invisible);
    x.f(o + 0x1a, m.index);
    x.f(o + 0x1b, m.pad1b);
    x.f(o + 0x1c, m.f1c);
    x.f(o + 0x20, m.f20);
    x.f(o + 0x24, m.oldX);
    x.f(o + 0x28, m.oldY);
    x.f(o + 0x2c, m.f2c);
    XAnim(x, o + 0x30, m.anim, &m.model, orig + 0x6c);
    x.heap(o + 0x6c, m.model);
    x.f(o + 0x70, m.frame);
    x.f(o + 0x74, m.f74);
    x.ptr(o + 0x78, m.stats, gMonsterStats, 46, kOrigStats, 0xa0);
    x.f(o + 0x7c, m.rect);
    x.f(o + 0x8c, m.hp);
    x.f(o + 0x90, m.action);
    x.f(o + 0x91, m.timer);
    x.f(o + 0x92, m.aiMode);
    x.f(o + 0x93, m.pad93);
    x.f(o + 0x94, m.angle);
    x.f(o + 0x98, m.moveAngle);
    x.f(o + 0x9c, m.drops);
    x.f(o + 0xa8, m.lastSeenX);
    x.f(o + 0xac, m.lastSeenY);
    x.f(o + 0xb0, m.destX);
    x.f(o + 0xb4, m.destY);
    x.f(o + 0xb8, m.detour);
    x.f(o + 0xbc, m.detourAngle);
    x.f(o + 0xc0, m.angleToPlayer);
    x.f(o + 0xc4, m.savedAngle);
    x.f(o + 0xc8, m.chaseDist);
    x.ptr(o + 0xcc, m.spawn, gSpawns, 1250, kOrigSpawns, 0x1c);
    x.f(o + 0xd0, m.holdTime);
    x.f(o + 0xd4, m.fearTime);
    x.f(o + 0xd8, m.rangedTimer);
    x.f(o + 0xd9, m.deathTimer);
    x.f(o + 0xda, m.killedByPlayer);
    x.f(o + 0xdb, m.padDB);
    x.f(o + 0xdc, m.xp);
}

void XServant(Xfer &x, int o, Servant &s, int idx)
{
    uint32_t orig = kOrigServants + idx * 0x80;
    x.f(o + 0x00, s.active);
    x.f(o + 0x04, s.x);
    x.f(o + 0x08, s.y);
    x.f(o + 0x0c, s.angle);
    x.f(o + 0x10, s.sx);
    x.f(o + 0x14, s.sy);
    x.f(o + 0x18, s.action);
    x.f(o + 0x1c, s.counter);
    x.f(o + 0x20, s.mode);
    x.f(o + 0x24, s.targetIdx);
    x.f(o + 0x28, s.wanderAngle);
    x.ptr(o + 0x2c, s.target, gMonsters, 1250, kOrigMonsters, 0xe0);
    x.f(o + 0x30, s.destX);
    x.f(o + 0x34, s.destY);
    x.f(o + 0x38, s.life);
    XAnim(x, o + 0x3c, s.anim, &s.model, orig + 0x78);
    x.heap(o + 0x78, s.model);
    x.f(o + 0x7c, s.f7c);
}

void XConvNode(Xfer &x, int o, ConvNode &n)
{
    x.f(o + 0x00, n.topic);
    x.f(o + 0x04, n.state);
    x.f(o + 0x08, n.answer);
    x.f(o + 0x0c, n.flags);
    x.f(o + 0x10, n.costText);
    x.f(o + 0x14, n.cost);
    x.ptr(o + 0x18, n.next, gConvNodes, 50, kOrigConvNodes, 0x1c);
}

// Scalars scattered over a few original addresses (unknown bytes are 0).
struct Scalar { int off; void *p; int size; };

void XScalars(Xfer &x, const Scalar *s, int n)
{
    for (int i = 0; i < n; i++) x.raw(s[i].off, s[i].p, s[i].size);
}

// One save block: original size and a converter.
void XBlock(Xfer &x, int block)
{
#define POD(arr, n)                                                     \
    do {                                                                \
        static_assert(sizeof(arr) >= (n), "save block larger than data"); \
        x.raw(0, (void *)&(arr), (n));                                  \
    } while (0)
    switch (block) {
    case 0: POD(gStats, 0x64); break;
    case 1: POD(gPlayer, 0xbc); break;
    case 2: POD(gSaveMisc, 0xa0); break;
    case 3: POD(gAutomap, 0x3f7a); break;
    case 4: POD(gEquip, 0x24); break;
    case 5: POD(gInventory, 0x78); break;
    case 6: POD(gSpellsMemorized, 0x1e); break;
    case 7: POD(gRuneCounts, 0x1a); break;
    case 8: POD(gLevelItems, 0x5208); break;
    case 9: POD(gDecals, 0x20d0); break;
    case 10: POD(gFeatures, 0x222e); break;
    case 11: POD(gDoors, 0x1c20); break;
    case 12:
        for (int i = 0; i < 1250; i++) XMonster(x, i * 0xe0, gMonsters[i], i);
        break;
    case 13: POD(gProjectiles, 0x230); break;
    case 14: POD(gContainers, 0xe10); break;
    case 15: {
        static const Scalar s[] = {
            {0x0, &g_5a6590, 1}, {0x1, &gMetJetraal, 1}, {0x3, &g_5a6593, 1},
            {0x4, &gTalkedToPriest, 1}, {0x5, &gMetGremlins, 1}, {0x6, &gGremlinsGotOrb, 1},
            {0x8, &g_5a6598, 4}, {0xc, &g_5a659c, 2}, {0xe, &g_5a659e, 1},
        };
        if (x.save) memset(x.buf, 0, 0xf);
        XScalars(x, s, sizeof s / sizeof s[0]);
        break;
    }
    case 16: POD(gTrigWalls, 0x870); break;
    case 17: POD(gTriggers, 0x23a0); break;
    case 18: POD(gTrigDoors, 0x78); break;
    case 19: {
        static const Scalar s[] = {
            {0x0, &gLightSpell, 4}, {0x4, &gLightKind, 4}, {0xc, &gLightFrame, 4},
            {0x10, &gLightSx, 4}, {0x14, &gLightSy, 4},
        };
        if (x.save) memset(x.buf, 0, 0x18);
        XScalars(x, s, sizeof s / sizeof s[0]);
        break;
    }
    case 20:
        for (int i = 0; i < 10; i++) XServant(x, i * 0x80, gServants[i], i);
        break;
    case 21: POD(gShopItems, 0x90); break;
    case 22: POD(gWarps, 0xfa0); break;
    case 23: POD(gLevelObjs, 0x2bc0); break;
    case 24: POD(gLevelAux, 0xc80); break;
    case 25: POD(gLights, 0x2ee0); break;
    case 26: POD(gWounds, 0xf0); break;
    case 27:
        for (int i = 0; i < 50; i++) XConvNode(x, i * 0x1c, gConvNodes[i]);
        break;
    case 28: POD(gSpellHotkeys, 0x28); break;
    }
#undef POD
}

// 0x45b5f8: block sizes in the file
const int kBlockSize[29] = {
    0x64, 0xbc, 0xa0, 0x3f7a, 0x24, 0x78, 0x1e, 0x1a, 0x5208, 0x20d0,
    0x222e, 0x1c20, 0x445c0, 0x230, 0xe10, 0xf, 0x870, 0x23a0, 0x78, 0x18,
    0x500, 0x90, 0xfa0, 0x2bc0, 0xc80, 0x2ee0, 0xf0, 0x578, 0x28,
};

// The raw-copied structures must have the original layout.
static_assert(sizeof(PlayerStats) == 0x64, "PlayerStats layout");
static_assert(sizeof(Player) == 0xbc, "Player layout");
static_assert(sizeof(Equipment) == 0x24, "Equipment layout");
static_assert(sizeof(InvSlot) == 4, "InvSlot layout");
static_assert(sizeof(LevelItem) == 0x0e, "LevelItem layout");
static_assert(sizeof(Decal) == 0x0e, "Decal layout");
static_assert(sizeof(Feature) == 0x0a, "Feature layout");
static_assert(sizeof(Door) == 0x12, "Door layout");
static_assert(sizeof(Projectile) == 0x38, "Projectile layout");
static_assert(sizeof(Container) == 0x48, "Container layout");
static_assert(sizeof(TrigWall) == 0x0c, "TrigWall layout");
static_assert(sizeof(TriggerGroup) == 0x98, "TriggerGroup layout");
static_assert(sizeof(TrigDoor) == 0x0c, "TrigDoor layout");
static_assert(sizeof(WarpTile) == 0x14, "WarpTile layout");
static_assert(sizeof(LevelObj) == 0x38, "LevelObj layout");
static_assert(sizeof(LevelAux) == 0x08, "LevelAux layout");
static_assert(sizeof(Light) == 0x10, "Light layout");
static_assert(sizeof(Wound) == 0x18, "Wound layout");
static_assert(sizeof(SaveHeader) == 0x40, "SaveHeader layout");

void SaveFileName(char *buf, int slot)
{
    strcpy(buf, "SAVEX.AE");
    buf[4] = (char)('0' + slot);
}

} // namespace

// ---- dialogs ----------------------------------------------------------------------

// 0x42ab40
void SaveGameDialog()
{
    char name[21];
    PCX pcx;
    Sprite *save = GrabScreen(0xb4, 0x46, 0x12c, 0x14a);
    pcx.Init(gDDW, (char *)"gamedat\\sgame.pcx", 0);
    pcx.Display(0xb4, 0x46, gDDW.MakePixel16(0, 0xff, 0));
    AddDirtyRect(0xb4, 0x46, 0x12c, 0x14a);
    UpdateAndRestore(&gDDW);
    ReadSaveHeaders();
    int slot = SaveSlotMenu();
    RestoreScreen(0xb4, 0x46, save);
    AddDirtyRect(0xb4, 0x46, 0x12c, 0x14a);
    UpdateAndRestore(&gDDW);
    if (slot != -1) {
        char *desc = gSaveHeaders[slot].desc;
        snprintf(name, sizeof name, "%s", desc);
        if (EditSaveName(name)) {
            if (name[0]) snprintf(desc, sizeof gSaveHeaders[slot].desc, "%s", name);
            SaveGame(slot);
        }
    }
    gDDW.FillRect(0, 0, 0x27f, 0x1df, 0);
}

// 0x42ad00: read the headers of SAVE0..9 into the slot menu.
void ReadSaveHeaders()
{
    char name[16];
    gSaveMenuCount = 11;
    for (int i = 0, y = 0x82; i < 10; i++, y += 0x19) {
        SaveHeader &h = gSaveHeaders[i];
        SaveFileName(name, i);
        int fd = w_open(name, W_O_RDONLY | W_O_BINARY);
        if (fd >= 0) {
            int n = w_read(fd, &h, 0x40);
            w_close(fd);
            if (n != 0x40) FatalError("Error reading :%s", name);
            else if (strcmp(h.magic, "AE1") != 0 || h.version != 0x10002) h.desc[0] = 0;
        } else {
            h.desc[0] = 0;
        }
        MenuItem &m = gSaveMenu[i];
        m.text = h.desc;
        m.id = i;
        m.y = y;
        m.top = y;
        m.bottom = y + 0x18;
        m.color = gColorWhite;
    }
}

// 0x42ae40
int SaveSlotMenu()
{
    PumpMessages();
    int r = RunMenu(gSaveMenu, 11, gColorRed, nullptr, 1);
    KeyClear();
    return r;
}

// 0x42ae70: type a name for the save. Returns 0 on escape.
int EditSaveName(char *s)
{
    int len = 0, done = 0;
    KeyClear();
    Sprite *save = GrabScreen(0xa0, 0xdc, 0x12c, 0x28);
    if (s[0]) len = (int)strlen(s);
    do {
        PumpMessages();
        save->Blt(gDDW, 0xa0, 0xdc);
        gText.PrintC(0xdc, (char *)gMsg[49] /* ENTER SAVE GAME NAME */, gColorWhite);
        uint8_t key[2];
        if (KeyPop(key)) {
            int c = (int8_t)key[0];
            if ((c >= 0 && isalnum(c)) || c == ' ') {
                if (!len) s[0] = (char)toupper(c);
                else s[len] = (char)tolower(c);
                s[len + 1] = 0;
                if (len < 16) len++;
            } else if (c == 8) {
                if (len > 0) s[--len] = 0;
            } else if (c == 0xd) {
                if (len) done = 1;
            } else if (c == 0x1b) {
                RestoreScreen(0xc8, 0xf0, save);    // sic: not where it was grabbed
                return 0;
            }
        }
        gText.Print(gText.StringSize(s) + 0xc8, 0xf0, (char *)"@", gColorWhite);   // cursor glyph
        gText.Print(0xc8, 0xf0, s, gColorGrey128);
        gDDW.UpdateScreen();
    } while (!done);
    RestoreScreen(0xa0, 0xdc, save);
    KeyClear();
    return 1;
}

// 0x42b270: encrypt and write one block.
static void WriteBlock(int fd, int slot, int block)
{
    int n = kBlockSize[block];
    uint8_t *buf = new uint8_t[n];
    Xfer x{true, buf};
    XBlock(x, block);
    SaveHeader &h = gSaveHeaders[slot];
    for (int i = 0; i < n; i++) buf[i] ^= h.key[i % h.keyLen];
    w_write(fd, buf, n);
    delete[] buf;
}

// 0x42b820: read and decrypt one block.
static void ReadBlock(int fd, int slot, int block)
{
    int n = kBlockSize[block];
    uint8_t *buf = new uint8_t[n];
    memset(buf, 0, n);
    w_read(fd, buf, n);
    SaveHeader &h = gSaveHeaders[slot];
    for (int i = 0; i < n; i++) buf[i] ^= h.key[i % h.keyLen];
    Xfer x{false, buf};
    XBlock(x, block);
    delete[] buf;
}

// 0x42b2f0
static void WriteLevel(int fd, int level)
{
    int size = gLevelW[level] * gLevelH[level];
    w_write(fd, &size, 4);
    w_write(fd, gLevelLayout[level], size);
    w_write(fd, gLevelTiles[level], size);
    w_write(fd, gLevelWalls[level], size);
    w_write(fd, gLevelAltWalls[level], size);
}

// 0x42b8a0
static void ReadLevel(int fd, int level)
{
    int size = 0;
    w_read(fd, &size, 4);
    w_read(fd, gLevelLayout[level], size);
    w_read(fd, gLevelTiles[level], size);
    w_read(fd, gLevelWalls[level], size);
    w_read(fd, gLevelAltWalls[level], size);
}

// 0x42b020
void SaveGame(int slot)
{
    char name[16];
    SaveHeader &h = gSaveHeaders[slot];
    gText.SetColor(gColorWhite, 0);
    gText.PrintC(0xf0, (char *)gMsg[50] /* SAVING.. */);
    gText.PrintC(0x104, h.desc);
    AddDirtyRect(0, 0x104, 0x280, 0x28);
    UpdateAndRestore(&gDDW);
    strcpy(h.magic, "AE1");
    h.version = 0x10002;
    SaveFileName(name, slot);
    h.keyLen = (uint8_t)(rand() % 27 + 5);
    for (int i = 0; i < 32; i++) h.key[i] = (uint8_t)(rand() % 192 + 0x20);

    int32_t *m = gSaveMisc;
    m[1] = (int32_t)gTicks;
    m[0] = gCurLevel;
    m[2] = gHaveMap;
    m[4] = gUIMode;
    m[7] = gStatuesDestroyed;
    m[3] = gTorchFuel;
    m[5] = gActionMode;
    m[6] = g_5bc194;
    m[10] = gDrainTime;
    m[8] = gTime;
    m[9] = gRegenTime;
    m[13] = (int32_t)gFrameCounter;
    m[11] = gNumLights;
    m[12] = gTimeStop;
    m[14] = gDeepestLevel;
    m[15] = g_5ad910;

    int fd = w_open(name, W_O_BINARY | W_O_TRUNC | W_O_CREAT | W_O_RDWR, 0600);
    if (fd < 0) {
        FatalError("Wont open :%s", name);
        return;
    }
    w_write(fd, &h, 0x40);
    for (int b = 0; b < 29; b++) WriteBlock(fd, slot, b);
    for (int l = 0; l < 25; l++) WriteLevel(fd, l);
    w_close(fd);
}

// 0x42b380: returns 1 if a game was loaded.
int LoadGameDialog()
{
    PCX pcx;
    int done = 0, slot;
    ShowMouse(0);
    Sprite *save = GrabScreen(0xb4, 0x46, 0x118, 0x14a);
    pcx.Init(gDDW, (char *)"gamedat\\lgame.pcx", 0);
    pcx.Display(0xb4, 0x46, gDDW.MakePixel16(0, 0xff, 0));
    AddDirtyRect(0xb4, 0x46, 0x118, 0x14a);
    UpdateAndRestore(&gDDW);
    ShowMouse(1);
    ReadSaveHeaders();
    do {
        slot = SaveSlotMenu();
        if (slot == -1 || SaveExists(slot)) done = 1;
    } while (!done);
    RestoreScreen(0xb4, 0x46, save);
    AddDirtyRect(0xb4, 0x46, 0x118, 0x14a);
    UpdateAndRestore(&gDDW);
    if (slot == -1) {
        gDDW.FillRect(0, 0, 0x27f, 0x1df, 0);
        return 0;
    }
    LoadGame(slot);
    UpdateLightRadius();
    UpdateLighting();
    gDDW.FillRect(0, 0, 0x27f, 0x1df, 0);
    ResetMouseClicks();
    return 1;
}

// 0x42b540
int SaveExists(int slot)
{
    char name[16];
    if (slot == -1) return 0;
    SaveFileName(name, slot);
    int fd = w_open(name, W_O_RDONLY | W_O_BINARY);
    if (fd <= 0) return 0;
    w_close(fd);
    return 1;
}

// 0x42b5b0
void LoadGame(int slot)
{
    char name[16];
    ShowMouse(0);
    SaveFileName(name, slot);
    LeaveGame();
    int fd = w_open(name, W_O_RDONLY | W_O_BINARY);
    if (fd < 0) {
        FatalError("WONT OPEN : %s", name);
        return;
    }
    SaveHeader &h = gSaveHeaders[slot];
    w_read(fd, &h, 0x40);
    if (strcmp(h.magic, "AE1") != 0) FatalError("NOT A SAVE FILE : %s", name);
    if (h.version != 0x10002) FatalError("INCORRECT SAVE FILE VERSION : %s", name);
    if (!h.keyLen) h.keyLen = 1;    // guard against a corrupt header
    for (int b = 0; b < 29; b++) ReadBlock(fd, slot, b);
    for (int l = 0; l < 25; l++) ReadLevel(fd, l);
    w_close(fd);

    int32_t *m = gSaveMisc;
    gHaveMap = m[2];
    gActionMode = m[5];
    gTime = m[8];
    gDrainTime = m[10];
    gCurLevel = m[0];
    gFrameCounter = (uint32_t)m[13];
    gCamX = gPlayer.x;
    memcpy(gTraps, gTrapsInit, sizeof gTrapsInit);
    gTicks = (uint32_t)m[1];
    gUIMode = m[4];
    gTorchFuel = m[3];
    g_5bc194 = m[6];
    gStatuesDestroyed = m[7];
    gLoadTime = gTime;
    gRegenTime = m[9];
    gNumLights = m[11];
    gTimeStop = m[12];
    gDeepestLevel = m[14];
    g_5ad910 = m[15];
    gCamY = gPlayer.y;
    UpdateLightRadius();
    RelinkMonsters();
    LinkConversations();
    EnterLevel(gCurLevel);
    ClearDeadMonsters();
    UpdateWalkSpeed();
    RelinkServants();
    g_5c5824 = 1;
    ShowMouse(1);
}
