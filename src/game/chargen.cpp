// Intro, title and loading screens, character generation and FatalError
// (0x4139c0-0x414e20).
#include "game.h"

// 0x4139c0
int PlayIntro()
{
    gDDW.PageFlipping(0);
    ShowMouse(0);
    if (!PlayIntroMovie()) return 0;
    DrawTitleScreen();
    gDDW.PageFlipping(1);
    ShowMouse(1);
    return 1;
}

// 0x413a10: title picture (drawn into both buffers).
void DrawTitleScreen()
{
    PCX pcx;
    gText.SetColor(gColorGrey96, 0);
    pcx.Init(gDDW, (char *)"gamedat\\title.pcx", 0);
    for (int i = 0; i < 2; i++) {
        pcx.Display(0, 0);
        gText.PrintRJ(0x276, 0x1b8, (char *)gMsg[0] /* v1.11 */);
        gText.PrintRJ(0x276, 0x1cc, (char *)gMsg[1] /* (Software) */);
        if (i == 0) gDDW.UpdateScreen();
    }
    pcx.Release();
}

// 0x413b30
void ShowLoadingScreen()
{
    PCX pcx;
    ShowMouse(0);
    pcx.Init(gDDW, (char *)"gamedat\\loading.pcx", 0);
    pcx.Display(0xb7, 0xc0, gDDW.MakePixel16(0, 0xff, 0) & 0xffff);
    gDDW.UpdateScreen();
    pcx.Display(0xb7, 0xc0, gDDW.MakePixel16(0, 0xff, 0) & 0xffff);
    pcx.Release();
    ShowMouse(1);
}

// 0x413c90 / 0x413cc0 (static initialisers)
void StaticInit_CGModels()
{
    for (int i = 0; i < 4; i++) gCGModels[i].Construct();
}
void StaticInit_CGTex()
{
    for (int i = 0; i < 4; i++) gCGTex[i].Construct();
}

// 0x413ce0: returns 1 when a new character was created.
int CharacterGeneration()
{
    static MenuItem items[2];
    static bool init = false;
    if (!init) {
        init = true;
        MenuItem a = {nullptr, (uint16_t)gColorWhite, 'Y', 0, (uint16_t)gColorAzure, 0, 1, 0x172, 0x1b8, 0x172, 0x1b8, 0x1a4, 0x1cb};
        MenuItem b = {nullptr, (uint16_t)gColorWhite, 'N', 0, (uint16_t)gColorAzure, 0, 0, 0x226, 0x1b8, 0x226, 0x1b8, 0x258, 0x1cb};
        items[0] = a;
        items[1] = b;
    }
    static const char *omt[4] = {"gamedat\\cgw.omt", "gamedat\\cgm.omt", "gamedat\\cgr.omt", "gamedat\\cgt.omt"};
    static const char *tex[4] = {"gamedat\\cgw.tex", "gamedat\\cgm.tex", "gamedat\\cgr.tex", "gamedat\\cgt.tex"};
    int result = 0;
    gUIMode = 10;
    gDDW.SetClippingWindowSize(0, 0, 0x27f, 0x1df);
    gViewHeight = 0x1df;
    if (!gChargenPCX.Init(gDDW, (char *)"gamedat\\chargen.pcx", 1)) FatalError("Cannot init char_gen splash");
    items[0].text = gMsg[160] /* |Yes */;
    items[1].text = gMsg[161] /* |No */;
    for (int i = 0; i < 4; i++) {
        gCGModels[i].Load(omt[i]);
        gCGTex[i].Load(tex[i]);
        gCGModels[i].SetTexture(&gCGTex[i]);
        gCGModels[i].SetScale(1.6f);
    }
    DrawChargen(0);
    UpdateAndRestore(&gDDW);
    int sel = 0;
    if (EnterName()) {
        for (;;) {
            sel = 0;
            if (ChooseClass() && RollStats()) {
                gText.SetColor(gColorWhite, 0);
                gText.PrintS(0x172, 0xf0, (char *)gMsg[131] /* Will this profile suffice ? */, 1);
                AddDirtyRect(0x172, 0xf0, gText.StringSize((char *)gMsg[131] /* Will this profile suffice ? */), 0x10);
                do {
                    sel = RunMenu(items, 2, gColorRed, nullptr, 0);
                } while (sel == -1);
            }
            if (sel != 0) break;
            DrawChargen(0);
            UpdateAndRestore(&gDDW);
            if (!EnterName()) break;
        }
    }
    if (sel != 0) {
        // Create the new character.
        memset(&gPlayer, 0, sizeof(gPlayer));
        memset(&gEquip, 0, sizeof(gEquip));
        memset(gRuneCounts, 0, 0x1a);
        memset(gSpellsMemorized, 0, 0x1e);
        for (int i = 0; i < 30; i++) {
            gInventory[i].item = -1;
            gInventory[i].qty = 0;
        }
        gInventory[1].qty = 10000;
        gInventory[2].qty = 10000;
        gInventory[0].item = 0x71;
        gHaveMap = 1;
        gInventory[1].item = 0x81;
        gInventory[2].item = 0x81;
        gInventory[3].item = 0x83;
        gInventory[4].item = 0x87;
        gInventory[5].item = 0x88;
        switch (gStats.charClass) {
        case 0:
            gInventory[6].item = 0x36;
            gInventory[7].item = 0x46;
            gEquip.armourType = 2;
            break;
        case 1:
            gInventory[6].item = 0x48;
            gRuneCounts[0] = 1;
            gRuneCounts[1] = 1;
            gRuneCounts[4] = 1;
            gRuneCounts[7] = 1;
            gRuneCounts[0xd] = 1;
            gRuneCounts[0x13] = 1;
            gRuneCounts[2] = 1;
            gRuneCounts[0x10] = 1;
            break;
        case 2:
            gInventory[6].item = 0x35;
            gPlayer.arrows = 0xf;
            gEquip.armourType = 1;
            gRuneCounts[1] = 1;
            gRuneCounts[0xd] = 1;
            gRuneCounts[0x13] = 1;
            gInventory[7].item = 0x32;
            break;
        case 3:
            gInventory[6].item = 0x8d;
            gInventory[7].item = 0x32;
            break;
        }
        gEquip.e0c = 1;
        result = 1;
    }
    gUIMode = 0;
    gChargenPCX.Release();
    gViewHeight = 0x18f;
    for (int i = 0; i < 4; i++) {
        gCGModels[i].Free();
        gCGTex[i].Free();
    }
    return result;
}

// 0x414180: redraw the character generation screen (bit 1 name, bit 0
// class and portrait, bit 2 statistics) into both buffers.
void DrawChargen(int flags)
{
    ShowMouse(0);
    for (int n = 2; n; n--) {
        gChargenPCX.Display(0, 0);
        gText.SetColor(gColorWhite, 0);
        if (flags & 2) gText.PrintS(0x78, 0x96, gStats.name, 1, 2);
        if (flags & 1) {
            gText.PrintS(0x78, 0xaa, (char *)gMsg[208 + gStats.charClass], 1, 2);
            gFaces[gStats.portrait].Draw(5, 0x83, gDDW);
        }
        if (flags & 4) DrawStats(0x50, 0x118);
        gDDW.UpdateScreen();
    }
    ShowMouse(0);
}

static Sprite *gClassBack;       // 0x4be62c

// 0x414270
int ChooseClass()
{
    static MenuItem items[4];
    static bool init = false;
    if (!init) {
        init = true;
        uint16_t w = (uint16_t)gColorWhite, az = (uint16_t)gColorAzure;
        MenuItem a = {nullptr, w, 'W', 0, az, 0, 0, 0x17c, 0xf0, 0x17c, 0xf0, 0x276, 0x103};
        MenuItem b = {nullptr, w, 'S', 0, az, 0, 1, 0x17c, 0x10e, 0x17c, 0x10e, 0x276, 0x12b};
        MenuItem c = {nullptr, w, 'R', 0, az, 0, 2, 0x17c, 0x12c, 0x17c, 0x12c, 0x276, 0x13f};
        MenuItem d = {nullptr, w, 'T', 0, az, 0, 3, 0x17c, 0x14a, 0x17c, 0x14a, 0x276, 0x15d};
        items[0] = a;
        items[1] = b;
        items[2] = c;
        items[3] = d;
    }
    gText.SetColor(gColorWhite, 0);
    gText.PrintS(0x168, 0xb4, (char *)gMsg[125] /* Choose thy career */, 1);
    gDDW.TintRect(0x50, 0xf0, 0xa0, 0xc8, gColorGrey128);
    gClassBack = GrabScreen(0x46, 0xf0, 0xb4, 0xc8);
    AddDirtyRect(0x168, 0xb4, gText.StringSize((char *)gMsg[125] /* Choose thy career */), 0x14);
    AddDirtyRect(0x46, 0xf0, 0xb4, 0xc8);
    for (int i = 0; i < 4; i++) items[i].text = gMsg[204 + i];
    AddDirtyRect(0x17c, 0xf0, 0xfa, 0x78);
    gShade.SetShadeLevel(0x1f);
    gStats.charClass = RunMenu(items, 4, gColorRed, ChargenIdle, 1);
    RestoreScreen(0x46, 0xf0, gClassBack);
    if (gStats.charClass == -1) return 0;
    gStats.portrait = gStats.charClass;
    DrawChargen(3);
    return 1;
}

static int gCGPrevSel = -1;      // 0x449850
static int gCGDir = 5;           // 0x449854
static int gCGAngle;             // 0x4be898
static uint32_t gCGFrame;        // 0x4be89c

// 0x414570: menu idle callback: spin the model of the highlighted class.
void ChargenIdle(int sel, int, int)
{
    if (sel != gCGPrevSel) {
        gCGPrevSel = sel;
        gCGFrame = 0;
        gCGDir = 5;
        gCGAngle = 0;
    }
    gClassBack->Blt(gDDW, 0x46, 0xf0);
    if (sel < 0) return;
    Model *m = &gCGModels[sel];
    if (!m->numFrames) return;   // (port: not loaded; numFrames - 1 below would wrap)
    m->SetLightRange(0x1f);
    if (gCGFrame >= m->numFrames - 1) gCGFrame = 0;
    uint32_t f = gCGFrame++;
    m->Draw(0xa0, 0x190, f, (float)gCGAngle);
    gCGAngle += gCGDir;
    if (gCGAngle > 0x50 || gCGAngle < -0x50) gCGDir = -gCGDir;
}

// 0x414640
int EnterName()
{
    gDDW.TintRect(0x15e, 0xd2, 0x118, 0x46, gDDW.MakePixel16(0x60, 0x60, 0x60));
    gText.SetColor(gColorWhite, 0);
    gText.PrintS(0x168, 0xdc, (char *)gMsg[129] /* What whilst thou be known as ? */, 1);
    AddDirtyRect(0x15e, 0xd2, 0x118, 0x46);
    gStats.name[0] = 0;
    if (!TextInput(gStats.name)) return 0;
    DrawChargen(2);
    return 1;
}

// 0x4146e0: edit a name of up to 19 characters.
int TextInput(char *buf)
{
    int len = 0;
    int done = 0;
    KeyClear();
    Sprite *saved = GrabScreen(0x163, 0xfa, 0x122, 0x14);
    char *p = buf;
    for (;;) {
        PumpMessages();
        saved->Blt(gDDW, 0x163, 0xfa);
        uint8_t k[2];
        if (KeyPop(k)) {
            int c = (signed char)k[0];
            if (isalnum(c) || c == ' ') {
                if (len == 0 || (len > 0 && isspace((signed char)p[-1])))
                    *p = (char)toupper(c);
                else
                    *p = (char)tolower(c);
                buf[len + 1] = 0;
                if (len < 0x13 && gText.StringSize(buf) < 0xb9) {
                    len++;
                    p++;
                }
            } else if (c == 8) {
                if (len > 0) {
                    len--;
                    p--;
                    *p = 0;
                }
            } else if (c == 0xd) {
                if (len) done = 1;
            } else if (c == 0x1b) {
                RestoreScreen(0x15e, 0xfa, saved);
                buf[0] = 0;
                return 0;
            }
        }
        gText.Print(gText.StringSize(buf) + 0x17c, 0xfa, (char *)"@", gColorWhite);
        gText.Print(0x17c, 0xfa, buf, gColorGrey128);
        UpdateAndRestore(&gDDW);
        if (done) break;
    }
    RestoreScreen(0x163, 0xfa, saved);
    KeyClear();
    ResetMouseClicks();
    return 1;
}

// 0x414890
int RollStats()
{
    static MenuItem items[2];
    static bool init = false;
    if (!init) {
        init = true;
        uint16_t w = (uint16_t)gColorWhite, az = (uint16_t)gColorAzure;
        MenuItem a = {nullptr, w, 'R', 0, az, 0, 0, 0x226, 0x1b8, 0x226, 0x1b8, 0x258, 0x1cb};
        MenuItem b = {nullptr, w, 'O', 0, az, 0, 1, 0x172, 0x1b8, 0x172, 0x1b8, 0x190, 0x1cb};
        items[0] = a;
        items[1] = b;
    }
    int ok = 0;
    Sprite *saved = GrabScreen(0x15e, 0xe6, 0x118, 0xaa);
    items[0].text = gMsg[212] /* |Re-roll */;
    items[1].text = gMsg[213] /* |Ok */;
    for (;;) {
        PumpMessages();
        RollCharacter();
        gText.SetColor(gColorWhite, 0);
        gText.PrintS(0x168, 0xb4, (char *)gMsg[126] /* Determining thy abilities */, 1);
        DrawStats(0x17c, 0xe6);
        AddDirtyRect(0x15e, 0xb4, 0x118, 0xdc);
        int sel = RunMenu(items, 2, gColorRed, nullptr, 1);
        if (sel == 1)
            ok = 1;
        else if (sel == -1) {
            RestoreScreen(0x15e, 0xe6, saved);
            return 0;
        }
        saved->Blt(gDDW, 0x15e, 0xe6);
        if (ok) break;
    }
    RestoreScreen(0x15e, 0xe6, saved);
    DrawChargen(7);
    return 1;
}

// 0x414ad0
void RollCharacter()
{
    gStats.str = gStats.baseStr = Roll4to7();
    gStats.tou = gStats.baseTou = Roll4to7();
    gStats.dex = gStats.baseDex = Roll4to7();
    gStats.intel = gStats.baseInt = Roll4to7();
    gStats.magicXp = 0;
    gStats.xp = 0;
    gStats.maxSpellLevel = 1;
    gStats.level = 1;
    ClassBonus();
    gStats.maxHp = gStats.hp = gStats.baseTou + gStats.baseStr + 10;
    gStats.maxMana = gStats.mana = gStats.baseInt * 2;
}

// 0x414b60
void DrawStats(int x, int y)
{
    char vals[6][10];
    gText.SetColor(gColorAzure, 0);
    for (int i = 0; i < 6; i++) gText.PrintS(x, y + i * 20, (char *)gMsg[214 + i], 1);
    gText.SetColor(gColorWhite, 0);
    sprintf(vals[0], "%d", gStats.str);
    sprintf(vals[1], "%d", gStats.tou);
    sprintf(vals[2], "%d", gStats.dex);
    sprintf(vals[3], "%d", gStats.intel);
    sprintf(vals[4], "%d", gStats.maxHp);
    sprintf(vals[5], "%d", gStats.maxMana);
    for (int i = 0; i < 6; i++) gText.PrintS(x + 200, y + i * 20, vals[i], 1);
}

// 0x414c90
int Roll4to7()
{
    return rand() % 4 + 4;
}

// 0x414cb0
void ClassBonus()
{
    switch (gStats.charClass) {
    case 0:
        gStats.baseStr += 2;
        gStats.baseTou += 1;
        break;
    case 1:
        gStats.baseInt += 4;
        break;
    case 2:
        gStats.baseDex++;
        gStats.baseInt++;
        gStats.baseStr++;
        break;
    case 3:
        gStats.baseDex += 2;
        gStats.baseStr++;
        break;
    }
    gStats.str = gStats.baseStr;
    gStats.dex = gStats.baseDex;
    gStats.tou = gStats.baseTou;
    gStats.intel = gStats.baseInt;
}

// 0x414d50
void FatalError(const char *fmt, ...)
{
    char msg[0x100], text[0x100], caption[0x100];
    msg[0] = 0;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    snprintf(caption, sizeof(caption), "Error at time index =%d", gTicks);
    snprintf(text, sizeof(text), "%s\nError No: %d\nPlease press a key", msg, 0);
    fprintf(stderr, "Ancient Evil: %s\n", msg);
    gDDW.Flip2GDI();
    plat_message_box(text, caption);
    gRunning = 0;
    GameShutdown();
    exit(0xd);
}
