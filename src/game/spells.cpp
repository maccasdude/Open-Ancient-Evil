// Spell memorising and casting menus (0x41a8e0-0x41b310).
#include "game.h"

static inline int MemorizeLimit(int spell)
{
    return gMemorizeLimit[gStats.maxSpellLevel * 6 + spell / 5];
}

static void LayoutItem(MenuItem *m, int x, int y)
{
    m->x = x;
    m->left = x;
    m->y = y;
    m->top = y;
    m->right = gText.StringSize((char *)m->text) + m->left;
    m->bottom = m->top + 0x10;
}

// 0x41a8e0
int CanMemorize(int spell)
{
    int grp = spell / 5;
    int limit = MemorizeLimit(spell);
    if (grp > gStats.maxSpellLevel - 1) return 0;
    return gSpellsMemorized[spell] < limit;
}

// 0x41a930: the "memorise spells" screen.
void MemorizeDialog()
{
    char flags[0x178 - 0x10];
    MenuItem local[15];
    if (CountMonsters(flags, 0)) {
        ShowMessage(gMsg[86] /* Monsters in range !! */, gColorRed);
        return;
    }
    gUIMode = 0xb;
    SetCursor(0);
    RedrawGameScreen(-1);
    AddDirtyRect(0, 0, 640, 480);
    UpdateAndRestore(&gDDW);
    int done = 0;
    do {
        gRunesPCX.Display(0x127, 0x57, gDDW.MakePixel16(0, 0xff, 0));
        gText.Print(0x159, 0x9b, (char *)gMsg[264] /* Select spell to memorize : */, gColorWhite);
        MenuItem *menu = local;
        int count = 0;
        int flash = 1;
        switch (gMemorizePage) {
        case 0: {
            menu = gSpellLevelMenu;
            count = gStats.maxSpellLevel + 1;
            gSpellLevelMenu[0].color = gColorWhite;
            gSpellLevelMenu[0].hiColor = gColorAzure;
            gSpellLevelMenu[0].text = gMsg[243] /* Done (|E|S|C) */;
            for (int k = 1, y = 0xcf; k < count; k++, y += 0x16) {
                MenuItem *m = &gSpellLevelMenu[k];
                m->color = gColorWhite;
                m->hiColor = gColorAzure;
                m->text = gMsg[237 + k - 1];
                LayoutItem(m, 0x159, y);
            }
            flash = 0;
            break;
        }
        case 1: count = BuildMemorizeMenu(0, 4, local); break;
        case 2: count = BuildMemorizeMenu(5, 9, local); break;
        case 3: count = BuildMemorizeMenu(10, 14, local); break;
        case 4: count = BuildMemorizeMenu(15, 19, local); break;
        case 5: count = BuildMemorizeMenu(20, 24, local); break;
        case 6: count = BuildMemorizeMenu(25, 29, local); break;
        }
        AddDirtyRect(0x122, 0x55, 0x15e, 0x145);
        int r = RunMenu(menu, count, gColorRed, nullptr, flash);
        if (r == -1)
            done = 1;
        else if (r == -9)
            gMemorizePage = 0;
        else if (r < -1)
            gMemorizePage = -1 - r;
        else if (CanMemorize(r))
            gSpellsMemorized[r]++;
    } while (!done);
    gUIMode = 0;
}

// 0x41ac00: list the spells of one level; returns the number of menu items.
int BuildMemorizeMenu(int first, int last, MenuItem *menu)
{
    char buf[20];
    int k = 0, rows = 0, total = 0;
    int cap = gStats.baseInt - (int)((float)gStats.maxSpellLevel * -1.5f);
    for (int s = first; s <= last; s++) total += gSpellsMemorized[s];
    int y = 0xb9;
    for (int s = first; s <= last; s++) {
        const int32_t *r = &gSpellRunes[s * 3];
        if (!gRuneCounts[r[0]] || !gRuneCounts[r[1]] || !gRuneCounts[r[2]]) continue;
        int limit = MemorizeLimit(s);
        if (gSpellsMemorized[s] < limit && total < cap) {
            MenuItem *m = &menu[k++];
            m->text = gSpellNames[s];
            m->id = s;
            m->color = gColorWhite;
            m->hotkey = 0;              // (left uninitialised by the original)
            m->hiColor = gColorAzure;
            LayoutItem(m, 0x159, y);
            gText.SetColor(gColorWhite, 0);
        } else {
            gText.Print(0x159, y, (char *)gSpellNames[s], gColorGrey96);
        }
        sprintf(buf, "%d/%d", (int)gSpellsMemorized[s], MemorizeLimit(s));
        gText.PrintRJ(0x258, y, buf);
        rows++;
        y += 0x16;
    }
    sprintf(buf, gMsg[265] /* %d of %d remaining */, cap - total, cap);
    gText.SetColor(gColorWhite, 0);
    gText.PrintS(0x159, 0x168, buf, gColorNearBlack, 2);

    MenuItem *m = &menu[k++];
    m->text = gMsg[173] /* |Change spell level */;
    m->id = -9;
    m->color = gColorGrey128;
    m->hiColor = gColorAzure;
    m->hotkey = 'C';
    LayoutItem(m, 0x159, rows * 22 + 0xb9);
    m = &menu[k];
    m->text = gMsg[75] /* Done memorizing spells (|E|S|C) */;
    m->id = -1;
    m->color = gColorGrey128;
    m->hiColor = gColorAzure;
    m->hotkey = 0x1b;
    LayoutItem(m, 0x159, rows * 22 + 0xcf);
    return k + 1;
}

// 0x41af30: choose the spell to cast.
void CastDialog()
{
    MenuItem local[15];
    gUIMode = 6;
    gDDW.SetClippingWindowSize(0, 0, 0x27f, 0x1df);
    RedrawGameScreen(2);
    SetCursor(0);
    AddDirtyRect(0, 0, 640, 480);
    UpdateAndRestore(&gDDW);
    int r;
    do {
        gRunesPCX.Display(0x127, 0x57, gDDW.MakePixel16(0, 0xff, 0));
        gText.Print(0x159, 0x9b, (char *)gMsg[153] /* Select An Option : */, gColorWhite);
        if (gCastPage > 6) gCastPage = 0;
        MenuItem *menu = local;
        int count = 0;
        switch (gCastPage) {
        case 0:
            menu = gCastMenu;
            count = gStats.maxSpellLevel + 2;
            for (int k = 0; k < count; k++) {
                gCastMenu[k].color = k < 2 ? gColorGrey128 : gColorWhite;
                gCastMenu[k].hiColor = gColorAzure;
                gCastMenu[k].text = gMsg[235 + k];
            }
            AddDirtyRect(0x127, 0x55, 0x15e, 0x145);
            break;
        case 1: count = BuildCastMenu(0, 4, local); break;
        case 2: count = BuildCastMenu(5, 9, local); break;
        case 3: count = BuildCastMenu(10, 14, local); break;
        case 4: count = BuildCastMenu(15, 19, local); break;
        case 5: count = BuildCastMenu(20, 24, local); break;
        case 6: count = BuildCastMenu(25, 29, local); break;
        }
        for (int i = 0, y = 0xb9; i < count; i++, y += 0x16) LayoutItem(&menu[i], 0x159, y);
        r = RunMenu(menu, count, gColorRed, nullptr, 0);
        if (r == -10) {
            CancelSpellDialog();
            r = -1;
        } else if (r == -9) {
            gCastPage = 0;
        } else if (r < -1) {
            gCastPage = -1 - r;
        } else if (r >= 0) {
            gPlayer.currentSpell = r;
            r = -1;
        }
    } while (r != -1);
    gDDW.FillRect(0, 0, 0x27f, 0x18f, 0);
    gUIMode = 0;
}

// 0x41b1e0: memorised spells of one level; returns the number of items.
int BuildCastMenu(int first, int last, MenuItem *menu)
{
    char buf[16];
    int k = 0;
    int y = 0xb9;
    for (int s = first; s <= last; s++) {
        if (!gSpellsMemorized[s]) continue;
        win_itoa(gSpellsMemorized[s], buf, 10);
        gText.Print(0x23f, y, buf, gColorWhite);
        y += 0x16;
        MenuItem *m = &menu[k++];
        m->text = gSpellNames[s];
        m->id = s;
        m->color = gColorWhite;
        m->hotkey = 0;
        m->hiColor = gColorAzure;
    }
    MenuItem *m = &menu[k];
    m->text = gMsg[173] /* |Change spell level */;
    m->id = -9;
    m->color = gColorGrey128;
    m->hiColor = gColorAzure;
    m->hotkey = 'C';
    m = &menu[k + 1];
    m->text = gMsg[174] /* Done casting (|E|S|C) */;
    m->id = -1;
    m->color = gColorGrey128;
    m->hiColor = gColorAzure;
    m->hotkey = 0x1b;
    AddDirtyRect(0x127, 0x55, 0x15e, 0x145);
    return k + 2;
}
