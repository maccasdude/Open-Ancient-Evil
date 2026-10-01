// Using decorations (stairs, signs, chests...), the health and mana bars,
// experience and the chest screens (0x425dd0-0x427630).
#include "game.h"

static int FeatureOf(int vi) { return gVisFeatures[vi].feature; }

// 0x425dd0
void UseFeature(int vi)
{
    switch (gFeatureNameIdx[gVisFeatures[vi].sprite]) {
    case 1: case 25: ReadSign(vi); break;
    case 2: ShowMessage(gMsg[284] /* The lid is too heavy for a mortal to lif ... */, gColorWhite); break;
    case 4: case 48: case 84:
        PullLeverA(vi);
        StartTurn();
        break;
    case 5: case 49: case 85:
        PullLeverB(vi);
        StartTurn();
        break;
    case 6: UseStairsDown(vi); break;
    case 7: UseStairsUp(vi); break;
    case 14: case 17: case 18: OpenChest(vi); break;
    case 15: Rest(); break;
    case 16: ReadBookshelf(vi); break;
    case 44: RingBell(vi); break;
    case 57: case 58: TalkToMushroom(); break;
    case 79: OfferToStatue(vi); break;
    case 93: FallStairsUp(vi); break;
    case 97: FallStairsDown(vi); break;
    default: ShowMessage(gMsg[30] /* You have no use for that now */, gColorWhite); break;
    }
    gUIMode = 0;
}

// 0x426010
int FeatureClickable(int vi)
{
    switch (gFeatureNameIdx[gVisFeatures[vi].sprite]) {
    case 1: case 25: case 57: case 58:
        return 1;
    case 4: case 5: case 6: case 7: case 14: case 15: case 16: case 17: case 18: case 44:
    case 48: case 49: case 79: case 84: case 85: case 93: case 97:
        return FeatureInReach(vi) ? 1 : 0;
    }
    return 0;
}

// 0x4260f0
void UseObject(int i)
{
    if (gLevelObjs[i].kind == 0x17) {
        DrinkFountain(i);
        return;
    }
    ShowMessage(gMsg[30] /* You have no use for that now */, gColorWhite);
}

static void TakeLink(int vi, const char *msg)
{
    Feature &f = gFeatures[FeatureOf(vi)];
    int x = f.x, y = f.y;
    int ox = gPlayer.tileX, oy = gPlayer.tileY;
    if (!FeatureInReach(vi)) {
        ShowMessage(gMsg[2] /* It is too far away. */, gColorRed);
        return;
    }
    ShowMessage(msg, gColorWhite);
    LeaveGame();
    ChangeLevelAt(x, y);
    EnterLevel(gCurLevel);
    ShiftServants(ox, oy, gPlayer.tileX, gPlayer.tileY);
    g_5c5824 = 1;
}

// 0x426130
void UseStairsDown(int vi) { TakeLink(vi, gMsg[22] /* Going down... */); }

// 0x4261f0
void UseStairsUp(int vi) { TakeLink(vi, gMsg[23] /* Going up... */); }

// 0x4262b0: move the player through the level link at (x,y).
void ChangeLevelAt(int x, int y)
{
    for (int i = 0; gLevelLinks[i].level != -1; i++) {
        LevelLink &l = gLevelLinks[i];
        if (l.level != gCurLevel || l.x != x || l.y != y) continue;
        gPlayer.x = gCamX = (float)l.destX + 0.5f;
        gCurLevel = l.destLevel;
        gPlayer.y = gCamY = (float)l.destY + 0.5f;
        WorldToScreen(gPlayer.x, gPlayer.y, &gPlayer.sx, &gPlayer.sy);
        return;
    }
}

// 0x426360
int FeatureInReach(int vi)
{
    Feature &f = gFeatures[FeatureOf(vi)];
    return DistanceB((float)f.x + 0.5f, (float)f.y + 0.5f, gPlayer.x, gPlayer.y) < 2.5f;
}

// 0x4263e0
void ReadSign(int vi)
{
    char buf[52];
    Feature &f = gFeatures[FeatureOf(vi)];
    int x = f.x, y = f.y;
    if (gLightMap[x * 65 + y] < 8) {
        ShowMessage(gMsg[223] /* It is too dark to read */, gColorWhite);
        return;
    }
    if (gCurLevel == 0xe && x == 0x32 && y == 0x27) {
        strcpy(buf, gMsg[24] /* It reads : */);
        strcat(buf, gStats.name);
        ShowMessage(buf, gColorAzure);
        strcpy(buf, gMsg[208 + gStats.charClass]);
        ShowMessage(buf, gColorAzure);
        return;
    }
    for (int i = 0; gSigns[i].level != -1; i++) {
        SignText &s = gSigns[i];
        if (s.level != gCurLevel || s.x != x || s.y != y) continue;
        strcpy(buf, gMsg[24] /* It reads : */);
        strcat(buf, s.text1);
        ShowMessage(buf, gColorAzure);
        if (s.text2[0]) ShowMessage(s.text2, gColorAzure);
        return;
    }
}

static void Flash()
{
    PlaySound(0x22, -1, -1);
    gDDW.FillRect((RECT *)nullptr, gColorWhite);
    gDDW.UpdateScreen();
}

// 0x4265f0: a trap door
void FallStairsDown(int vi)
{
    Flash();
    UseStairsDown(vi);
}

// 0x426630
void FallStairsUp(int vi)
{
    Flash();
    UseStairsUp(vi);
}

// 0x426670: redraw the health and mana bars when they changed.
void UpdatePoisonDisplay(int force)
{
    gText.SetFont(&gFontSmall);
    gText.AntiAlias(0);
    gText.SetColor(gColorWhite, 0);
    if (gStats.hp <= 0) gStats.hp = 0;
    float hp = (float)gStats.hp;
    if (gShownHp != hp || force) {
        gShownHp = hp;
        gShownMaxHp = (float)gStats.maxHp;
        DrawHpBar();
    }
    if (gStats.mana <= 0) gStats.mana = 0;
    float mana = (float)gStats.mana;
    if (gShownMana != mana || force) {
        gShownMana = mana;
        gShownMaxMana = (float)gStats.maxMana;
        DrawManaBar();
    }
    gText.SetFont(&gFontLarge);
    gText.AntiAlias(1);
}

// 0x426760
void DrawHpBar()
{
    char buf[16];
    unsigned c = gDDW.MakePixel16(0xc0, 0, 0);
    if (gPlayer.diseased) c = gDDW.MakePixel16(0xff, 0xc0, 0x80);
    if (gPlayer.poisoned) c = gDDW.MakePixel16(0, 0xff, 0);
    if (gShownHp != 0.0f) {
        int h = (int)(gShownHp / gShownMaxHp * 68.0f);
        if (h) gDDW.TintRect(0x1b, 0x1d9 - h, 0xe, h, (uint16_t)c);
    }
    sprintf(buf, "%d", gStats.hp);
    gText.PrintRJ(0x19, 0x1d1, buf);
}

// 0x426850
void DrawManaBar()
{
    char buf[16];
    if (gShownMana != 0.0f) {
        int h = (int)(gShownMana / gShownMaxMana * 68.0f);
        if (h) gDDW.TintRect(0x2c, 0x1d9 - h, 0xe, h, gDDW.MakePixel16(0, 0, 0xff));
    }
    sprintf(buf, "%d", gStats.mana);
    gText.Print(0x3c, 0x1d1, buf);
}

// 0x4268f0: 'H'ealth report.
void ShowHealth()
{
    char buf[40], num[12];
    strcpy(buf, gMsg[25] /* Your current health is : */);
    win_itoa(gStats.hp, num, 10);
    strcat(buf, num);
    strcat(buf, gMsg[26] /* of */);
    win_itoa(gStats.maxHp, num, 10);
    strcat(buf, num);
    ShowMessage(buf, gColorRed);
    ShowFatigue();
}

// 0x4269f0
void ShowMana()
{
    char buf[40], num[12];
    strcpy(buf, gMsg[76] /* Your spell energy is : */);
    win_itoa(gStats.mana, num, 10);
    strcat(buf, num);
    strcat(buf, gMsg[26] /* of */);
    win_itoa(gStats.maxMana, num, 10);
    strcat(buf, num);
    ShowMessage(buf, gColorAzure);
}

// 0x426af0
void ShowFatigue()
{
    int i = 0;
    while (i < 5 && gPlayer.fatigue >= gFatigueLevels[i]) i++;
    ShowMessage(gMsg[100 + i], gColorWhite);
}

// 0x426b30
void GainExperience(int xp)
{
    gStats.xp += xp;
    if (gStats.xp <= gXpTable[gStats.level]) return;
    gStats.level++;
    PlaySound(9, -1, -1);
    if (gStats.level % 2) {
        gStats.str = ++gStats.baseStr;
        if (gStats.baseStr > 0x14) gStats.baseStr = 0x14;
        gStats.dex = ++gStats.baseDex;
        if (gStats.baseDex > 0x14) gStats.baseDex = 0x14;
        gStats.tou = ++gStats.baseTou;
        if (gStats.baseTou > 0x14) gStats.baseTou = 0x14;
        gStats.intel = ++gStats.baseInt;
        if (gStats.baseInt > 0x14) gStats.baseInt = 0x14;
    }
    int gain = rand() % gHpDice[gStats.charClass] + 1;
    if (gStats.hp == gStats.maxHp) gStats.hp += gain;
    gStats.maxHp += gain;
    RecalcStats();
    ShowMessage(gMsg[29] /* You have gained an EXPERIENCE level */, gColorGreen);
    UpdatePoisonDisplay(1);
}

// 0x426c50
void GainMagicExperience(int xp)
{
    static const int32_t kMagicXp[7] = {0, 300, 875, 1800, 3250, 5250, 2000000}; // 0x44ca58
    gStats.magicXp += xp;
    if (gStats.magicXp <= kMagicXp[gStats.maxSpellLevel]) return;
    gStats.maxSpellLevel++;
    if (gStats.mana == gStats.maxMana) gStats.mana += gStats.baseInt;
    gStats.maxMana += gStats.baseInt;
    ShowMessage(gMsg[79] /* You have gained a SPELL CASTING level */, gColorGreen);
    UpdatePoisonDisplay(1);
}

// 0x426cd0: traps and hot floors under the player.
void CheckStandingTile()
{
    if (gPlayer.gameMode == 5) return;
    int x = gPlayer.tileX, y = gPlayer.tileY;
    switch (gLevelMap[gMapRow[y] + x]) {
    case 9:
        PressurePlate(x, y, 1);
        break;
    case 10:
        if ((unsigned)gFrameCounter % 3 == 0 || gEquip.e0c == 3) return;
        if (rand() % 100 < gStats.dex * 5) return;
        if (gEquip.e0c) {
            gEquip.e0c = 0;
            LoadPlayerTextures();
            RecalcStats();
        }
        DamagePlayer(rand() % 5 + 1, 0);
        ShowMessage(gMsg[111] /* Lava burns!!! */, gColorWhite);
        break;
    case 11:
        TrapGoesOff(x, y);
        break;
    }
}

static void ChestMessage(const char *m) { StatusText(m); }

// 0x426dc0
void OpenChest(int vi)
{
    Feature &f = gFeatures[FeatureOf(vi)];
    int x = f.x, y = f.y;
    if (!FeatureInReach(vi)) {
        ShowMessage(gMsg[2] /* It is too far away. */, gColorRed);
        return;
    }
    SetCursor(0);
    ShowMouse(0);
    int ci = 0;
    bool found = false;
    for (; gContainers[ci].level != -1; ci++) {
        Container &c = gContainers[ci];
        if (c.level == gCurLevel + 1)
            for (int k = 0; k < 3; k++)
                if (c.pos[k * 2] == x && c.pos[k * 2 + 1] == y) found = true;
        if (found) break;
    }
    if (!found) return;
    gStatusTimer = 0;
    gUIMode = 6;
    gSavedHudVisible = gHudVisible;
    gHudVisible = 1;
    gViewHeight = 0x18f;
    RedrawGameScreen(-1);
    AddDirtyRect(0, 0, 640, 480);
    UpdateAndRestore(&gDDW);
    if (!gChestPCX.Init(gDDW, (char *)"gamedat\\chest.pcx", 0)) return;
    gChestSpriteCount = LoadCSpriteTable((char *)"gamedat\\chest.cst", g_5a6448, 0x14);
    DrawChestContents(ci);
    gChestSave = GrabScreen(0xf5, 0xe6, 0x186, 0x50);
    SetStatusY(0x13d);
    if (ChestUnlock(&gContainers[ci])) {
        DrawChestContents(ci);
        ChestLootUI(ci);
        CompactInventory();
    }
    RestoreScreen(0xf5, 0xe6, gChestSave);
    FreeDirtyRects();
    ClearStatusText();
    gDDW.FillRect(0, 0, 0x27f, 0x1df, 0);
    gChestPCX.Release();
    ReleaseCSpriteTable(g_5a6448, gChestSpriteCount);
    gHudVisible = gSavedHudVisible;
    if (!gHudVisible) gViewHeight = 0x1df;
    ShowMouse(1);
    gUIMode = 0;
}

// 0x427000
void DrawChestContents(int ci)
{
    ShowMouse(0);
    gChestPCX.Display(0x5a, 0x46, gDDW.MakePixel16(0, 0xff, 0));
    DrawContainer(ci);
    AddDirtyRect(0x5a, 0x46, 0x1cc, 0x154);
    ShowMouse(1);
}

// 0x427060: returns 1 when the chest may be looted.
int ChestUnlock(Container *c)
{
    if (!c->closed) return 1;
    if (c->needsKey && FindInventoryItem(0x8d) == -1) {
        ShowMessage(gMsg[144] /* You require lockpicks for this chest */, gColorWhite);
        return 0;
    }
    ChestLockUI(c);
    if (c->closed || c->needsKey != c->b1f) return 0;
    if (c->trapped) {
        c->trapped = 0;
        TrapGoesOff(gPlayer.tileX, gPlayer.tileY);
        return 0;
    }
    return 1;
}

// 0x427100: open / examine / disarm buttons.
void ChestLockUI(Container *c)
{
    int done = 0;
    uint8_t key[2];
    do {
        PumpMessages();
        DrawStatusText();
        if (MouseLeftClicked()) {
            if (MouseInBox(0x1f6, 0x127, 0x11, 0x11)) done++;
            if (MouseInBox(0x100, 0xf8, 0x4a, 0x28)) {
                gStatusTimer = 0;
                if (c->closed == 2) {
                    c->closed = 0;
                    done++;
                }
            }
            if (MouseInBox(0x160, 0xf8, 0x4a, 0x28)) {
                // examine for traps
                const char *m;
                if (!c->examined) {
                    if (!c->trapped) {
                        m = gMsg[147] /* No trap found */;
                    } else if (rand() % 100 + 1 > gStats.intel * 5) {
                        m = gMsg[147] /* No trap found */;
                    } else {
                        c->examined = 1;
                        m = gMsg[150] /* You have found a trap */;
                    }
                } else {
                    m = c->trapped ? gMsg[146] /* Trap already found */ : gMsg[147] /* No trap found */;
                }
                ChestMessage(m);
            }
            if (MouseInBox(0x1bd, 0xf8, 0x4a, 0x28)) {
                // disarm
                if (!c->examined) {
                    ChestMessage(gMsg[147] /* No trap found */);
                } else if (c->trapped) {
                    c->trapped = 0;
                    if (rand() % 100 + 1 > (gStats.dex + 6) * 5) {
                        done++;
                        TrapGoesOff(gPlayer.tileX, gPlayer.tileY);
                    } else {
                        ChestMessage(gMsg[148] /* The trap has been disarmed */);
                    }
                } else {
                    ChestMessage(gMsg[149] /* The trap has already been disarmed */);
                }
            }
        }
        if (KeyPop(key) && key[0] == 0x1b) done++;
        gChestSave->Blt(gDDW, 0xf5, 0xe6);
        DrawHints();
        UpdateAndRestore(&gDDW);
    } while (!done);
}

// 0x4272c0: moving items between the chest and the backpack.
void ChestLootUI(int ci)
{
    bool done = false;
    uint8_t key[2] = {0, 0};
    Container &c = gContainers[ci];
    do {
        PumpMessages();
        KeyPop(key);
        gHudFlags = 0;
        DrawStatusText();
        if (MouseRightClicked()) {
            int item = -1;
            int slot = InventorySlotAtMouse();
            if (slot != -1 && gInventory[slot].item != -1) {
                item = gInventory[slot].item;
            } else {
                int cs = ChestSlotAtMouse();
                if (cs != -1 && c.items[cs].item != -1) item = c.items[cs].item;
            }
            if (item != -1)
                ChestMessage(gItemNames[gPlayer.identify ? gItemTrueNameIdx[item] : gItemNameIdx[item]]);
            DrawChestContents(ci);
            CompactInventory();
        }
        if (MouseLeftClicked() || key[0] == ' ') {
            InventoryArrows();
            gStatusTimer = 0;
            int slot = InventorySlotAtMouse();
            int cs;
            if (slot != -1 && gInventory[slot].item != 0) {
                PutInChest(ci, slot);
            } else if ((cs = ChestSlotAtMouse()) != -1) {
                TakeFromChest(ci, cs);
            } else if (MouseInBox(0x100, 0xf8, 0x4a, 0x28)) {
                c.closed = 2;
                done = true;
            } else if (MouseInBox(0x1f6, 0x127, 0x11, 0x11)) {
                done = true;
            }
            DrawChestContents(ci);
            CompactInventory();
        }
        if (key[0] == 0x1b) done = true;
        if (key[1] == 0x25)
            ScrollInventory(-1);
        else if (key[1] == 0x27)
            ScrollInventory(1);
        DrawHud();
        gChestSave->Blt(gDDW, 0xf5, 0xe6);
        AddDirtyRect(0x5a, 0x13d, 0x1cc, 0x14);
        DrawHints();
        UpdateAndRestore(&gDDW);
        key[0] = 0;
    } while (!done);
}

// 0x4274a0
int ChestSlotAtMouse()
{
    for (int i = 0; i < 8; i++)
        if (MouseInBox(gChestSlotX[i], gChestSlotY[i], 0x38, 0x38)) return i;
    return -1;
}

// 0x4274e0
void PutInChest(int ci, int slot)
{
    int cs = FreeChestSlot(ci);
    if (cs == -1) {
        ChestMessage(gMsg[53] /* There is no room in the chest for that */);
        return;
    }
    gContainers[ci].items[cs] = gInventory[slot];
    if (gInventory[slot].item == 0x71) gHaveMap = 0;
    gInventory[slot].item = -1;
}

// 0x427540
int FreeChestSlot(int ci)
{
    for (int i = 0; i < 8; i++)
        if (gContainers[ci].items[i].item == -1) return i;
    return -1;
}

// 0x427570
void TakeFromChest(int ci, int cs)
{
    InvSlot &s = gContainers[ci].items[cs];
    int item = s.item;
    if (item == 0x78 || item == 0x79) {
        gPlayer.gold += s.qty;
    } else if (item >= 0 && item <= 0x1d) {
        gRuneCounts[item]++;
    } else {
        int slot = FindFreeInvSlot();
        if (slot < 0) {
            ChestMessage(gMsg[54] /* No room to carry that */);
            return;
        }
        gInventory[slot] = s;
        if (CarriedWeight() > gStats.maxWeight) {
            ChestMessage(gMsg[200] /* Too heavy */);
            gInventory[slot].item = -1;
            return;
        }
    }
    s.item = -1;
    if (item == 0x71) gHaveMap = 1;
}
