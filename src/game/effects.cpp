// Player wound marks, spell hot keys and lightning bolt effects (0x402140-0x403680).
#include "game.h"

// ---------------------------------------------------------------------------
// Wounds drawn onto the player's skin texture

// 0x402140
void ClearWounds() { memset(gWounds, 0, sizeof(gWounds)); }

// 0x402160: add a random wound line (upper/lower half, left/right half)
void AddWound(int lower, int right) {
    int yoff = lower ? 0x40 : 0;
    int xoff = right ? 0x80 : 0;
    int i = 0;
    Wound *w = gWounds;
    while (w < gWounds + 10) {
        if (!w->active) break;
        w++;
        i++;
    }
    if (i == 10) return;
    w = &gWounds[i];
    do {
        w->x0 = rand() % 128 + xoff;
        w->y0 = rand() % 64 + yoff;
        w->x1 = rand() % 128 + xoff;
        w->y1 = rand() % 64 + yoff;
    } while (abs(w->x1 - w->x0) < 32 || abs(w->y1 - w->y0) < 16);
    w->color = gDDW.MakePixel16((unsigned char)(rand() % 64 - 0x5c), 0, 0);
    w->active = 1;
    DrawWounds();
}

// 0x402260: render all wounds into the player texture
void DrawWounds() {
    gPlayerTexture.GetPtr(&gDDW.surfacePtr, &gDDW.pitch);
    for (int i = 0; i < 10; i++) {
        Wound *w = &gWounds[i];
        if (w->active) DrawLine(w->x0, w->y0, w->x1, w->y1, w->color, 0);
    }
}

// 0x4022c0: heal wounds so that only 'keep' remain, then reload the texture
void HealWounds(int keep) {
    int n = CountWounds() - keep;
    if (n <= 0) return;
    for (int i = 0; i < 10; i++) {
        if (gWounds[i].active) {
            gWounds[i].active = 0;
            if (--n == 0) break;
        }
    }
    LoadPlayerTextures();
}

// 0x402300
int CountWounds() {
    int n = 0;
    for (int i = 0; i < 10; i++)
        if (gWounds[i].active) n++;
    return n;
}

// ---------------------------------------------------------------------------
// Spell hot keys

// 0x402370 (static initialiser)
void StaticInit_HotkeyMenu() {
    uint16_t white = gColorWhite, azure = gColorAzure;
    for (int i = 0; i < 10; i++) {
        MenuItem *m = &gHotkeyMenu[i];
        m->text = nullptr;
        m->color = white;
        m->hotkey = (char)(i == 9 ? '0' : '1' + i);
        m->hiColor = azure;
        m->id = i;
        m->x = 0xd7;
        m->y = 0x96;
        m->left = 0xb4;
        m->top = 0x96;
        m->right = 0x1cc;
        m->bottom = 0xa9;
    }
    MenuItem *m = &gHotkeyMenu[10];
    m->text = nullptr;
    m->color = white;
    m->hotkey = 0x1b;
    m->hiColor = azure;
    m->id = -1;
    m->x = -1;
    m->y = 0x96;
    m->left = 0x19a;
    m->top = 0x73;
    m->right = 0x1b8;
    m->bottom = 0x91;
}

// 0x4026e0
void ClearSpellHotkeys() {
    for (int i = 0; i < 10; i++) gSpellHotkeys[i] = -1;
    g_5ad910 = 0;
}

// 0x402700: hot key configuration screen
void HotkeyDialog() {
    RedrawGameScreen(3);
    gHotkeyPCX.Init(gDDW, (char *)"gamedat\\hotkeys.pcx", 1);
    AddDirtyRect(0, 0, 640, 480);
    UpdateAndRestore(&gDDW);
    int sel;
    do {
        gHotkeyPCX.Display(0xb4, 0x46, gDDW.MakePixel16(0, 0xff, 0));
        BuildHotkeyMenu();
        AddDirtyRect(0xb4, 0x46, 0x118, 0x154);
        UpdateAndRestore(&gDDW);
        sel = RunMenu(gHotkeyMenu, gHotkeySlots[gStats.charClass] + 1, gColorRed, 0, 1);
        if (sel == -1) break;
        HotkeySelectSpell(sel);
    } while (sel != -1);
    gHotkeyPCX.Release();
}

// 0x4027f0
void BuildHotkeyMenu() {
    int i;
    for (i = 0; i < gHotkeySlots[gStats.charClass]; i++) {
        if (i == 9) strcpy(gHotkeyText[9], gMsg[260] /* 1|0 */);
        else sprintf(gHotkeyText[i], " |%d ", i + 1);
        const char *name = gSpellHotkeys[i] == -1 ? gMsg[259] /* --- Unused --- */ : gSpellNames[gSpellHotkeys[i]];
        strcat(gHotkeyText[i], name);
        MenuItem *m = &gHotkeyMenu[i];
        m->text = gHotkeyText[i];
        m->y = i * 25 + 0x82;
        m->top = i * 25 + 0x82;
        m->bottom = i * 25 + 0x9a;
        m->color = gColorWhite;
        m->hiColor = gColorAzure;
    }
    int n = gHotkeySlots[gStats.charClass];
    if (n != 10) gHotkeyMenu[n] = gHotkeyMenu[10];
}

// 0x402940: choose the spell assigned to a hot key slot
void HotkeySelectSpell(int slot) {
    MenuItem items[11];
    memset(items, 0, sizeof items);
    Sprite *bg = GrabScreen(0x127, 0x57, 0x158, 0x139);
    AddDirtyRect(0xb4, 0x46, 0x118, 0x154);
    int sel;
    do {
        gRunesPCX.Display(0x127, 0x57, gDDW.MakePixel16(0, 0xff, 0));
        gText.Print(0x159, 0x9b, (char *)gMsg[153] /* Select An Option : */, gColorWhite);
        MenuItem *menu;
        int count;
        for (;;) {
            if ((unsigned)gHotkeyPage <= 6) break;
            gHotkeyPage = 0;
        }
        switch (gHotkeyPage) {
        case 0: {
            uint16_t white = gColorWhite;
            count = gStats.maxSpellLevel + 1;
            menu = gSpellLevelMenu;
            gSpellLevelMenu[0].color = white;
            gSpellLevelMenu[0].hiColor = gColorAzure;
            gSpellLevelMenu[0].text = gMsg[243] /* Done (|E|S|C) */;
            for (int k = 1; k < count; k++) {
                gSpellLevelMenu[k].color = white;
                gSpellLevelMenu[k].hiColor = gColorAzure;
                gSpellLevelMenu[k].text = gMsg[237 + k - 1] /* Level One (|1) ... */;
            }
            break;
        }
        case 1: count = BuildSpellMenu(0, 4, items); menu = items; break;
        case 2: count = BuildSpellMenu(5, 9, items); menu = items; break;
        case 3: count = BuildSpellMenu(10, 14, items); menu = items; break;
        case 4: count = BuildSpellMenu(15, 19, items); menu = items; break;
        case 5: count = BuildSpellMenu(20, 24, items); menu = items; break;
        default: count = BuildSpellMenu(25, 29, items); menu = items; break;
        }
        int y = 0xb9;
        for (int k = 0; k < count; k++) {
            MenuItem *m = &menu[k];
            m->left = 0x159;
            m->x = 0x159;
            m->top = y;
            m->y = y;
            y += 0x16;
            m->right = gText.StringSize((char *)m->text) + m->left;
            m->bottom = m->top + 0x10;
        }
        AddDirtyRect(0x122, 0x55, 0x15e, 0x145);
        sel = RunMenu(menu, count, gColorRed, 0, 0);
        if (sel == -9) {
            gHotkeyPage = 0;
        } else if (sel < -1) {
            gHotkeyPage = -1 - sel;
        } else if (sel >= 0) {
            gSpellHotkeys[slot] = sel;
            sel = -1;
        }
    } while (sel != -1);
    RestoreScreen(0x127, 0x57, bg);
    AddDirtyRect(0x127, 0x55, 0x15e, 0x145);
}

// 0x402bc0: menu of spells first..last plus "change level" and "done"
int BuildSpellMenu(int first, int last, MenuItem *items) {
    int n = 0;
    if (first <= last) {
        n = last - first + 1;
        MenuItem *m = items;
        for (int i = first; i <= last; i++, m++) {
            m->text = gSpellNames[i];
            m->id = i;
            m->color = gColorWhite;
        }
    }
    MenuItem *m = &items[n];
    m->text = gMsg[173] /* |Change spell level */;
    m->id = -9;
    m->color = gColorGrey128;
    m->hiColor = gColorAzure;
    m->hotkey = 'C';
    n++;
    m = &items[n];
    m->text = gMsg[243] /* Done (|E|S|C) */;
    m->id = -1;
    m->color = gColorGrey128;
    m->hiColor = gColorAzure;
    m->hotkey = 0x1b;
    return n + 1;
}

// 0x402c70: a hot key '1'..'0' was pressed
void UseSpellHotkey(int key) {
    char buf[52];
    int idx = key - '1';
    if (idx == -1) idx = 9;
    int spell = gSpellHotkeys[idx];
    if (spell == -1) return;
    if (gSpellsMemorized[spell] > 0) {
        gPlayer.currentSpell = spell;
        if (gActionMode == 1) return;
        gPrevActionMode = gActionMode;
        gActionMode = 1;
        gPlayer.actionFrame = 0;
        gPlayer.gameMode = 0x13;
        return;
    }
    sprintf(buf, gMsg[262] /* No %s spells memorized */, gSpellNames[spell]);
    ShowMessage(buf, gColorRed);
}

// 0x402d20
void Nullsub_402d20() {}

// ---------------------------------------------------------------------------
// Lightning bolts

// 0x402d30: sparks around a point
void SparkBurst(float x, float y) {
    for (int k = 4; k; k--) {
        int idx = FindFreeBolt();
        if (idx == -1) continue;
        int a1 = rand() % 360;
        Vec3 A;
        A.x = SinDeg(a1) * 0.4f + x;
        A.y = y - CosDeg(a1) * 0.4f;
        A.z = 20.0f;
        int a2 = rand() % 360;
        Vec3 B;
        B.x = SinDeg(a2) * 0.4f + x;
        B.y = y - CosDeg(a2) * 0.4f;
        B.z = 20.0f;
        gBolts[idx].Init(A, B, 5, 2);
    }
}

// 0x402e30: lightning striking down around the player
void LightningAroundPlayer() {
    int idx = FindFreeBolt();
    if (idx == -1) return;
    Vec3 A;
    A.x = gPlayer.x;
    A.y = gPlayer.y;
    A.z = (float)(rand() % 20) - -40.0f;
    int e = rand() % 360;
    Vec3 B;
    B.x = SinDeg(e) * 0.7f + gPlayer.x;
    B.y = gPlayer.y - CosDeg(e) * 0.7f;
    B.z = 0.0f;
    gBolts[idx].Init(A, B, 8, 3);
    gBolts[idx].SetDrift(0.0f, 0.0f, 1.0f);
}

// 0x402f30: lightning arc from the player to (x,y)
void LightningArc(float x, float y) {
    float px = gPlayer.x, py = gPlayer.y;
    Vec3 mid;
    mid.x = (px + x) * 0.5f;
    mid.y = (py + y) * 0.5f;
    mid.z = 60.0f;
    for (int k = 4; k; k--) {
        int idx = FindFreeBolt();
        if (idx != -1) {
            Vec3 from = {px, py, 40.0f};
            gBolts[idx].Init(from, mid, 10, 5);
        }
        idx = FindFreeBolt();
        if (idx != -1) {
            Vec3 to = {x, y, 10.0f};
            gBolts[idx].Init(mid, to, 10, 5);
        }
    }
}

// 0x403060: crackle on the ground at (x,y)
void GroundSpark(float x, float y) {
    int idx = FindFreeBolt();
    if (idx == -1) return;
    int e1 = rand() % 360;
    Vec3 A;
    A.x = SinDeg(e1) * 0.5f + x;
    A.y = y - CosDeg(e1) * 0.5f;
    A.z = 0.0f;
    int e2 = rand() % 360;
    Vec3 B;
    B.x = SinDeg(e2) * 0.5f + x;
    B.y = y - CosDeg(e2) * 0.5f;
    B.z = 0.0f;
    gBolts[idx].Init(A, B, 7, 5);
}

// 0x403150
void BoltBetween(Vec3 *from, Vec3 *to) {
    int idx = FindFreeBolt();
    if (idx == -1) return;
    gBolts[idx].Init(*from, *to, 4, 1);
}

// 0x4031b0: two short sparks in the direction 'angle'
void SparkDirected(float x, float y, float z, int angle) {
    for (int k = 2; k; k--) {
        int idx = FindFreeBolt();
        if (idx == -1) continue;
        Vec3 A = {x, y, z};
        int e = (rand() % 20 + angle - 10) % 360;
        if (e < 0) e += 360;
        Vec3 B;
        B.x = SinDeg(e) + x;
        B.y = y - CosDeg(e);
        B.z = z;
        gBolts[idx].Init(A, B, 4, 3);
    }
}

// 0x403290
void UpdateBolts() {
    for (Bolt *b = gBolts; b < gBolts + 30; b++)
        if (b->IsActive()) b->Update();
}

// 0x4032c0
void DrawBolts() {
    for (Bolt *b = gBolts; b < gBolts + 30; b++)
        if (b->IsActive()) b->Draw();
}

// 0x4032f0
int FindFreeBolt() {
    int i = 0;
    for (Bolt *b = gBolts; b < gBolts + 30; b++, i++)
        if (!b->IsActive()) return i;
    return -1;
}

// 0x403320
void Bolt::Init(Vec3 from, Vec3 to, int n, int lifetime) {
    if (n > 10) return;
    segments = n;
    a = from;
    drift.x = drift.y = drift.z = 0.0f;
    b = to;
    Update();
    life = lifetime;
}

// 0x4033d0: move and re-randomise the bolt
void Bolt::Update() {
    Vec3 na, nb;
    nb.x = b.x + drift.x;
    na.x = a.x + drift.x;
    na.y = a.y + drift.y;
    nb.y = b.y + drift.y;
    nb.z = b.z + drift.z;
    na.z = a.z + drift.z;
    float n1 = (float)(segments - 1);
    float dx = (nb.x - na.x) / n1;
    float dy = (nb.y - na.y) / n1;
    float dz = (nb.z - na.z) / n1;
    a = na;
    b = nb;
    pts[0] = a;
    int last = 1;
    if (segments - 1 > 1) {
        int cnt = segments - 2;
        Vec3 *p = &pts[1];
        for (int i = 0; i < cnt; i++, p++) {
            p->x = (float)(rand() % 200) * dx * 0.01f + p[-1].x;
            p->y = (float)(rand() % 200) * dy * 0.01f + p[-1].y;
            p->z = (float)(rand() % 200) * dz * 0.01f + p[-1].z;
        }
        last = cnt + 1;
    }
    pts[last] = b;
    life--;
}

// 0x403570
void Bolt::Draw() {
    FGObject o;
    memset(&o, 0, sizeof o);
    o.type = 0x100;
    for (int i = 0; i < segments - 1; i++) {
        Vec3 *p0 = &pts[i], *p1 = &pts[i + 1];
        WorldToScreen(p0->x, p0->y, &o.x0, &o.y0);
        WorldToScreen(p1->x, p1->y, &o.x1, &o.y1);
        o.x0 += 0x20;
        o.x1 += 0x20;
        o.y0 -= (int)p0->z;
        o.y1 -= (int)p1->z;
        o.depth = ScreenDepth(p0->x, p0->y);
        int b_ = rand() % 128 + 0x80;
        int g_ = rand() % 128 + 0x40;
        o.color = gDDW.MakePixel16(0x40, (unsigned char)g_, (unsigned char)b_);
        AddFGObject(&o);
    }
}
