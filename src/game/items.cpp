// Picking items up, placing them on the floor, the backpack and the
// options menus (0x420800-0x421f10).
#include "game.h"

// 0x420800: pick up the item(s) under the mouse.
int PickUpItems()
{
    int last = 0;
    int n = CollectPickables();
    if (!n) return 0;
    int remaining = n;
    for (;;) {
        int vi;
        if (n == 1) {
            vi = gPickList[0];
            last = 1;
        } else {
            int r = ChoosePickMenu(gPickList, remaining);
            if (r == -1) return 1;
            vi = gPickList[r];
            if (r < 9)
                for (int k = r; k < 9; k++) gPickList[k] = gPickList[k + 1];
            if (--remaining == 0) last = 1;
        }
        LevelItem &it = gLevelItems[gVisItems[vi].idx];
        int type = it.type;
        if (type == 0x78 || type == 0x79) {
            gPlayer.gold += it.fa;
        } else if (type == 0x7c) {
            if (it.fa == 0)
                gPlayer.arrows++;
            else
                gPlayer.arrows += it.fa;
        } else if (type < 0x1e) {
            gRuneCounts[type]++;
        } else {
            int slot = FindFreeInvSlot();
            if (slot < 0) {
                ShowMessage(gMsg[10] /* Your backpack is already full */, gColorRed);
                return 1;
            }
            gInventory[slot].item = (int16_t)type;
            gInventory[slot].qty = it.fa;
            if (CarriedWeight() > gStats.maxWeight) {
                ShowMessage(gMsg[200] /* Too heavy */, gColorRed);
                gInventory[slot].item = -1;
                return 1;
            }
            if (type == 0x71) gHaveMap = 1;
        }
        it.active = 0;
        if (gLevelMap[gMapRow[it.y] + it.x] == 5) ReleasePlate(it.x, it.y);
        ShowPickupMessage(type);
        g_5c5824 = 1;
        gPlayer.noise += 10;
        CompactInventory();
        if (last) return 1;
    }
}

// 0x420a60: put an item on the floor; coins stack. Returns the item index.
int DropItem(int x, int y, int type, int qty)
{
    uint8_t t = gLevelMap[gMapRow[y] + x];
    if ((t >= 6 && t <= 8) || (t >= 0xd && t <= 0xe)) return -1;
    if (t >= 0x14) return -1;
    int last = gItemLast;
    if (type == 0x78 || type == 0x79) {
        for (int i = gItemFirst; i <= last; i++) {
            LevelItem &it = gLevelItems[i];
            if (it.active != 1 || it.x != x || it.y != y || it.level != gCurLevel + 1) continue;
            if (it.type == 0x79) {
                if (type == 0x78)
                    it.fa++;
                else
                    it.fa += (int16_t)qty;
                return i;
            }
            if (it.type == 0x78) {
                it.type++;
                if (type == 0x78)
                    it.fa = 2;
                else
                    it.fa += (int16_t)qty;
                return i;
            }
        }
    }
    int i = 0;
    if (gLevelItems[0].active == 1) {
        do {
            if (++i == 1500) {
                fprintf(stderr, "info: too many items..\n");
                return -1;
            }
        } while (gLevelItems[i].active == 1);
    }
    int cell = t;
    if (cell == 2 || cell == 3) {
        // warp tile: the item lands at the destination
        WarpTile &w = gWarps[FindWarp(x, y)];
        x = w.destX;
        y = w.destY;
        cell = gLevelMap[gMapRow[y] + x];
    }
    if (cell == 4) {
        PressPlate(x, y);
        last = gItemLast;
    }
    LevelItem &it = gLevelItems[i];
    it.active = 1;
    it.type = (int16_t)type;
    it.level = (int16_t)(gCurLevel + 1);
    it.x = (int16_t)x;
    it.y = (int16_t)y;
    it.fa = (int16_t)qty;
    if (i < gItemFirst) gItemFirst = i;
    if (i > last) gItemLast = i;
    return i;
}

// 0x420ca0: visible items under the mouse within reach.
int CollectPickables()
{
    int n = 0;
    for (int i = 0; i < gNumVisItems; i++) {
        VisItem &v = gVisItems[i];
        if (v.idx == -1) continue;
        if (!MouseInRect((RECT *)v.rect)) continue;
        if (!IsPickable(v.idx)) continue;
        gPickList[n++] = i;
        if (n == 10) break;
    }
    return n;
}

static void InitMenuItem(MenuItem *m, int hotkey, int id, int x, int y, int l, int r)
{
    m->color = gColorWhite;
    m->hotkey = (int8_t)hotkey;
    m->hiColor = gColorAzure;
    m->id = id;
    m->x = x;
    m->y = y;
    m->left = l;
    m->top = y;
    m->right = r;
    m->bottom = y + 0x13;
}

// 0x420d10: menu of the items to pick up.
int ChoosePickMenu(int *list, int n)
{
    static bool init = false;
    static char names[11][50];
    if (!init) {
        init = true;
        for (int i = 0; i < 11; i++) {
            InitMenuItem(&gPickMenu[i], i < 9 ? '1' + i : (i == 9 ? '0' : 0x1b), 0, -1, 0xaa + i * 0x14, 0x78, 0x208);
            gPickMenu[i].text = nullptr;
        }
    }
    int i = 0;
    for (; i < n; i++) {
        MenuItem *m = &gPickMenu[i];
        m->id = i;
        m->hotkey = (int8_t)('1' + i);
        int type = gLevelItems[gVisItems[list[i]].idx].type;
        int ni = gPlayer.identify ? gItemTrueNameIdx[type] : gItemNameIdx[type];
        strcpy(names[i], gItemNames[ni]);
        names[i][0] = (char)toupper(names[i][0]);
        m->text = names[i];
    }
    if (i <= 0) return -1;
    MenuItem *m = &gPickMenu[i];
    m->hotkey = 0x1b;
    m->id = -1;
    m->text = gMsg[243] /* Done (|E|S|C) */;
    AddDirtyRect(0, 0, 640, 480);
    return RunMenu(gPickMenu, i + 1, gColorRed, nullptr, 0);
}

// 0x4211f0
int IsPickable(int idx)
{
    LevelItem &it = gLevelItems[idx];
    return DistanceB(gPlayer.x, gPlayer.y, (float)it.x + 0.5f, (float)it.y + 0.5f) < 2.25f;
}

// 0x421260: "<name> taken!" without the article.
void ShowPickupMessage(int type)
{
    char buf[40];
    const char *name = gItemNames[gItemNameIdx[type]];
    const char *p = name;
    if (*name != ' ') {
        const char *q = name;
        while (q[1] && q[1] != ' ') q++;
        p = q[1] ? q + 1 : name;
    }
    if (p != name) p++;
    strcpy(buf, p);
    strcat(buf, gMsg[5] /* taken! */);
    ShowMessage(buf, gColorWhite);
}

static inline int SortKey(int item) { return gInvSortKeyRaw[item + 1]; }

// 0x421310: pack the backpack and sort it.
void CompactInventory()
{
    int n;
    int i = 0;
    for (; i < 29; i++) {
        if (gInventory[i].item != -1) continue;
        int j = i + 1;
        while (j < 30 && gInventory[j].item == -1) j++;
        if (j >= 30) break;
        gInventory[i] = gInventory[j];
        gInventory[j].item = -1;
    }
    if (i >= 29) gInvCount = 30;
    n = gInvCount;
    for (int k = 0; k < 29; k++) {
        if (gInventory[k].item == -1) {
            n = k + 1;
            gInvCount = n;
            break;
        }
    }
    int m = n - 1;
    if (n == 30) m = n;
    for (int a = 0; a < m - 1; a++) {
        for (int b = a; b < m; b++) {
            int ia = gInventory[a].item, ib = gInventory[b].item;
            int ka = SortKey(ia), kb = SortKey(ib);
            if (ka > kb || (ka == kb && (int16_t)ia > (int16_t)ib)) {
                InvSlot t = gInventory[a];
                gInventory[a] = gInventory[b];
                gInventory[b] = t;
            }
        }
    }
}

// 0x421430
int FindFreeInvSlot()
{
    for (int i = 0; i < 30; i++)
        if (gInventory[i].item < 0) return i;
    return -1;
}

// 0x421460: the in-game options menu (ESC). Returns 2 to quit.
// Port: the sound, shadow, hints, always run, mouse and gamma entries moved
// into the Options screen (OptionsScreen); the menu keeps save, load, the
// spell hot keys and quitting.
int OptionsMenu()
{
    static bool init = false;
    enum { kN = 6 };
    static const int kKeys[kN] = {'S', 'R', 'O', 'K', 'A', 0x1b};
    char text[kN][40];
    if (!init) {
        init = true;
        for (int i = 0; i < kN; i++) InitMenuItem(&gOptionsMenu[i], kKeys[i], i, -1, 0x96 + i * 0x14, 0xdc, 0x1a4);
    }
    gUIMode = 4;
    RedrawGameScreen(3);
    Sprite *saved = GrabScreen(0x3c, 0x64, 0x208, 0x118);
    gText.SetColor(gColorWhite, 0);
    gText.PrintC(0x78, (char *)gMsg[153] /* Select An Option : */);
    AddDirtyRect(0, 0, 640, 480);
    strcpy(text[0], gMsg[154] /* |Save Game */);
    strcpy(text[1], gMsg[155] /* |Restore Game */);
    strcpy(text[2], "|Options");
    strcpy(text[3], gMsg[283] /* Set Spell Hot|keys */);
    strcpy(text[4], gMsg[159] /* |Abort Thy Quest */);
    strcpy(text[5], gMsg[169] /* Continue Thy Quest(|E|S|C) */);
    for (int i = 0; i < kN; i++) {
        gOptionsMenu[i].text = text[i];
        gOptionsMenu[i].hiColor = gColorAzure;
    }
    int r = RunMenu(gOptionsMenu, kN, gColorRed, nullptr, 1);
    RestoreScreen(0x3c, 0x64, saved);
    AddDirtyRect(0x3c, 0x64, 0x208, 0x118);
    switch (r) {
    case 0: SaveGameDialog(); break;
    case 1: LoadGameDialog(); break;
    case 2: OptionsScreen(true); break;
    case 3: HotkeyDialog(); break;
    case 4:
        if (QuitConfirm() == 1) return 2;
        break;
    }
    gUIMode = 0;
    FreeDirtyRects();
    gDDW.FillRect(0, 0, 0x27f, 0x1df, 0);
    return 0;
}

// 0x421b70
void ShadowOptionsMenu()
{
    static bool init = false;
    if (!init) {
        init = true;
        for (int i = 0; i < 4; i++)
            InitMenuItem(&gShadowMenu[i], i < 3 ? '0' + i : 0x1b, i < 3 ? i : -1, -1, 0x96 + i * 0x14, 0xdc, 0x1a4);
        memset(&gShadowMenu[4], 0, 4 * sizeof(MenuItem));
    }
    Sprite *saved = GrabScreen(0x3c, 0x96, 0x208, 0x96);
    for (int i = 0; i < 3; i++) {
        gShadowMenu[i].color = i == gPrefs.shadows ? gColorGrey128 : gColorWhite;
        gShadowMenu[i].text = gMsg[256 + i];
    }
    gShadowMenu[3].text = gMsg[168] /* Return To Game (|E|S|C) */;
    AddDirtyRect(0x3c, 0x96, 0x208, 0x96);
    int r = RunMenu(gShadowMenu, 8, gColorRed, nullptr, 1);
    RestoreScreen(0x3c, 0x96, saved);
    if (r != -1) gPrefs.shadows = r;
}

// 0x421da0
int QuitConfirm()
{
    Sprite *s = GrabScreen(0xe6, 0xc8, 0xd2, 0x3c);
    gText.SetColor(gColorWhite, 0);
    gText.PrintC(0xc8, (char *)gMsg[197] /* Quit really ? */);
    AddDirtyRect(0xa, 0xc8, 0x26c, 0x14);
    int r = YesNoMenu();
    RestoreScreen(0xe6, 0xc8, s);
    return r;
}

// 0x421e20
int YesNoMenu()
{
    MenuItem m[2];
    memset(m, 0, sizeof(m));
    m[0].color = gColorWhite;
    m[0].hotkey = 'Y';
    m[0].hiColor = gColorAzure;
    m[0].id = 1;
    m[0].x = 0xf0;
    m[0].y = 0xf0;
    m[0].left = 0xf0;
    m[0].top = 0xf0;
    m[0].right = 0x118;
    m[0].bottom = 0x103;
    m[1].color = gColorWhite;
    m[1].hotkey = 'N';
    m[1].hiColor = gColorAzure;
    m[1].id = 0;
    m[1].x = 0x177;
    m[1].y = 0xf0;
    m[1].left = 0x177;
    m[1].top = 0xf0;
    m[1].right = 0x190;
    m[1].bottom = 0x103;
    m[0].text = gMsg[160] /* |Yes */;
    m[1].text = gMsg[161] /* |No */;
    AddDirtyRect(0xf0, 0xf0, 0xa0, 0x14);
    int r = RunMenu(m, 2, gColorRed, nullptr, 1);
    return r == -1 ? 0 : r;
}
