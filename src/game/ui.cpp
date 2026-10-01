// Mouse handling in the different interface modes, the status bar buttons
// and dropping items (0x41f9f0-0x420800).
#include "game.h"

// 0x41f9f0
int ProcessMouse()
{
    MouseRead(&gMouseX, &gMouseY);
    switch (gUIMode) {
    case 0: GameMouse(); break;
    case 1: DropModeMouse(); break;
    case 2: InfoModeMouse(); break;
    case 3: UseModeMouse(); break;
    case 4: return OptionsMenu();
    }
    return -1;
}

// 0x41fa50: clicks in normal play.
void GameMouse()
{
    gHudFlags = 0;
    gLeftClick = MouseLeftClicked();
    if (ControlsOwnMouse())
        gCtlRightClick |= MouseRightClicked();   // port: the scheme uses the right button itself
    else if (gAttackRequest == 0)
        gAttackRequest = MouseRightClicked();
    if (gPlayer.gameMode != 7 && gLeftClick && MouseInHud() == 1) {
        int m = HudClick();
        gUIMode = m;
        gSavedHudVisible = gHudVisible;
        gLeftClick = 0;
        if (m) return;
    }
    if (ControlsOwnMouse()) return;
    switch (gActionMode) {
    case 1:
        if (gAttackRequest && gPlayer.gameMode == 0) {
            BeginCast(gPlayer.currentSpell);
            gAttackRequest = 0;
        }
        break;
    case 2:
        if (gPlayer.gameMode != 0) break;
        if (MouseButtonDown(1)) {
            if (gPlayer.jumpCharge != -1) gAttackRequest++;
        } else if (gPlayer.jumpCharge == -1) {
            gPlayer.jumpCharge = 0;
        }
        break;
    }
}

static int KeyCode()
{
    return gKeyBuf[0] ? (gKeyBuf[0] & 0x7f) : -1;
}

static int ArrowScroll()
{
    if (gKeyBuf[1] == 0x25) {
        ScrollInventory(-1);
        return 1;
    }
    if (gKeyBuf[1] == 0x27) {
        ScrollInventory(1);
        return 1;
    }
    return 0;
}

static void LeaveItemMode()
{
    gHudVisible = gSavedHudVisible;
    if (!gHudVisible) gViewHeight = 0x1df;
    gUIMode = 0;
    SetCursor(0);
    ResetMouseClicks();
}

// 0x41fb20: 'D'rop mode.
void DropModeMouse()
{
    gHudFlags = 0;
    int key = KeyCode();
    if (MouseRightClicked() || key == 0x1b) {
        LeaveItemMode();
        return;
    }
    if (MouseLeftClicked() || key == ' ') {
        if (InventoryArrows()) return;
        int slot = InventorySlotAtMouse();
        if (slot != -1) DropInventoryItem(slot);
        CompactInventory();
        if (gMouseY < 0x190) {
            LeaveItemMode();
            return;
        }
    }
    ArrowScroll();
}

// 0x41fbe0: 'U'se mode.
void UseModeMouse()
{
    gHudFlags = 0;
    int key = KeyCode();
    if (MouseRightClicked() || key == 0x1b) {
        LeaveItemMode();
        return;
    }
    if (!MouseLeftClicked() && key != ' ') {
        ArrowScroll();
        return;
    }
    if (InventoryArrows()) return;
    int i;
    if ((i = InventorySlotAtMouse()) != -1)
        UseInventoryItem(i);
    else if ((i = DoorUnderMouse()) != -1)
        UseDoor(i);
    else if ((i = MonsterUnderMouse()) != -1)
        TalkToMonster(i);
    else if ((i = FeatureUnderMouse()) != -1)
        UseFeature(i);
    else if ((i = ObjectUnderMouse()) != -1)
        UseObject(i);
    else
        ShowMessage(gMsg[6] /* Nothing to Use */, gColorRed);
    LeaveItemMode();
}

static void AddInfoLine(const char *s)
{
    strcpy(gInfoText[gInfoLines++], s);
}

static const char *ItemName(int type)
{
    int n = gPlayer.identify ? gItemTrueNameIdx[type] : gItemNameIdx[type];
    return gItemNames[n];
}

// 0x41fd10: 'I'nformation mode: names of things under the mouse.
void InfoModeMouse()
{
    int key = -1;
    gHudFlags = 0;
    gInfoLines = 0;
    gInfoX = gMouseX < 0x140 ? gMouseX + 0x14 : gMouseX - 0x14;
    if (gKeyBuf[0]) key = gKeyBuf[0] & 0x7f;
    if (MouseRightClicked() || key == 0x1b) {
        LeaveInfoMode();
        return;
    }
    if (MouseLeftClicked() || key == ' ') {
        if (InventoryArrows()) return;
        LeaveInfoMode();
        return;
    }
    if (gMouseY < 0x190) {
        ItemsUnderMouseInfo();
        int i = FeatureUnderMouse();
        if (i != -1) AddInfoLine(gFeatureNames[gFeatureNameIdx[gVisFeatures[i].sprite]]);
        i = ObjectUnderMouse();
        if (i != -1) AddInfoLine(gObjNames[gLevelObjs[i].kind]);
        i = DoorUnderMouse();
        if (i != -1) AddInfoLine(gDoorNames[gDoors[gVisDoors[i].door].kind]);
    } else {
        int slot = InventorySlotAtMouse();
        if (slot != -1 && gInventory[slot].item != -1) AddInfoLine(ItemName(gInventory[slot].item));
    }
    ArrowScroll();
}

void LeaveInfoMode()
{
    gUIMode = 0;
    SetCursor(0);
    ResetMouseClicks();
}

// 0x41ff50
int ItemsUnderMouseInfo()
{
    int found = 0;
    for (int i = 0; i < gNumVisItems; i++) {
        VisItem &v = gVisItems[i];
        if (!MouseInRect((RECT *)v.rect)) continue;
        AddInfoLine(ItemName(gLevelItems[v.idx].type));
        found = 1;
    }
    return found;
}

// 0x420020
void DrawInfoText(int)
{
    gText.SetColor(gColorWhite, 0);
    gInfoY = gMouseY;
    for (int i = 0; i < gInfoLines; i++) {
        if (gMouseX < 0x140)
            gText.Print(gInfoX, gInfoY, gInfoText[i]);
        else
            gText.PrintRJ(gInfoX, gInfoY, gInfoText[i]);
        gInfoY += gMouseY < 0xf0 ? 0xf : -0xf;
    }
}

// 0x4200d0 / 0x4200e0
int MouseInHud()
{
    return gHudVisible && gMouseY > gViewHeight + 1;
}

// 0x420110: a click on the status bar; returns the new interface mode.
int HudClick()
{
    if (gMouseY < 0x190 || gPlayer.gameMode) return 0;
    int b = HudButtonAt();
    if (gUIMode == 1 || gUIMode == 3) InventoryArrows();
    switch (b) {
    case 1:
        CompactInventory();
        SetCursor(3);
        return 1;
    case 2:
        CompactInventory();
        SetCursor(1);
        return 2;
    case 3:
        CompactInventory();
        SetCursor(4);
        return 3;
    case 4: return 4;
    case 7:
        gHudFlags |= 4;
        SearchSecrets();
        return 0;
    case 11: ShowHealth(); return 0;
    case 12: ShowMana(); return 0;
    case 14: MemorizeDialog(); return 0;
    case 15: InventoryScreen(); return 0;
    case 16:
        if (gActionMode == 1)
            CastDialog();
        else
            InventoryScreen();
        return 0;
    case 17: CycleActionMode(); return 0;
    }
    return 0;
}

static inline bool In(int v, int lo, int hi) { return v >= lo && v <= hi; }

// 0x420240
int HudButtonAt()
{
    int y = gMouseY, x = gMouseX;
    if (In(y, 0x1a7, 0x1c8)) {
        if (In(x, 0xaa, 0xdc)) return 1;
        if (In(x, 0xf0, 0x122)) return 3;
        if (In(x, 0x13b, 0x15e)) return 2;
        if (In(x, 0x172, 0x1ae)) return 7;
    }
    if (x >= 0x23f) {
        if (y < 0x190) goto l2f0;
        if (y <= 0x1ac) return 4;
    }
    if (y > 0x190) {
        if (In(x, 0x19, 0x2a)) return 0xb;
        if (In(x, 0x2c, 0x3c)) return 0xc;
    }
l2f0:
    if (In(x, 0x55, 0x78)) {
        if (In(y, 0x190, 0x1b7)) return 0xe;
        if (In(y, 0x1b8, 0x1df)) return 0xf;
    }
    if (x >= 0x23f && y >= 0x1b0) return 0x10;
    if (In(x, 0x1bd, 0x1ea) && In(y, 0x19f, 0x1d1)) return 0x11;
    return 0x63;
}

// 0x420370: the inventory scroll arrows on the status bar.
int InventoryArrows()
{
    int r = 0;
    int m = gUIMode;
    if (m != 1 && m != 3 && m != 5 && m != 2 && m != 6) return 0;
    if (MouseInBox(0x8a, 0x19c, 0x14, 0x19)) {
        ScrollInventory(-1);
        gHudFlags |= 1;
        r = 1;
    }
    if (MouseInBox(0x200, 0x19c, 0x14, 0x19)) {
        ScrollInventory(1);
        gHudFlags |= 2;
        r = 1;
    }
    return r;
}

// 0x420400: attack -> cast -> jump.
void CycleActionMode()
{
    if (++gActionMode >= 3) gActionMode = 0;
    UpdateLightRadius();
    FreePlayerModel();
    LoadPlayerModel();
}

// 0x420430
void ScrollInventory(int dir)
{
    int n = gInvCount;
    if (n < 5) {
        gInvScroll = 0;
        return;
    }
    int s = gInvScroll + dir * 5;
    if (s < 0) s = 0;
    gInvScroll = s;
    if (s >= n - 1) gInvScroll = ((n - 2) / 5) * 5;
}

// 0x420490
void DropInventoryItem(int slot)
{
    int16_t item = gInventory[slot].item;
    if (item == -1) return;
    if (item == 0x80) {
        ShowMessage(gMsg[74] /* The web is stuck */, gColorWhite);
        return;
    }
    int tx = (int)(SinDeg(gPlayer.angle) + gPlayer.x);
    int ty = (int)(gPlayer.y - CosDeg(gPlayer.angle));
    bool ok = CanDropAt(tx, ty);
    if (!ok) {
        tx = (int)gPlayer.x;
        ty = (int)gPlayer.y;
        ok = CanDropAt(tx, ty);
    }
    if (ok) {
        DropItem(tx, ty, gInventory[slot].item, gInventory[slot].qty);
        if (gInventory[slot].item == 0x71) gHaveMap = 0;
        ShowMessage(gMsg[12] /* Dropped!! */, gColorWhite);
        RemoveInventoryItem(slot);
        gPlayer.noise += 10;
    } else {
        ShowMessage(gMsg[11] /* No space to put that down */, gColorRed);
    }
    g_5c5824 = 1;
    CompactInventory();
    SortLevelItems();
    FindItemRange();
}

// 0x4205e0
int CanDropAt(int x, int y)
{
    int t = gLevelMap[gMapRow[y] + x];
    if (t >= 0x14) return 0;
    if (CountItemsAt(x, y) > 10) return 0;
    switch (t) {
    case 1: case 6: case 7: case 9: case 10: case 15:
        return 0;
    }
    return 1;
}

// 0x420760: active items first, ordered by level.
void SortLevelItems()
{
    for (int i = 0; i < 1499; i++) {
        for (int j = i; j < 1500; j++) {
            LevelItem &a = gLevelItems[i], &b = gLevelItems[j];
            if ((a.level > b.level && b.active) || (!a.active && b.active)) {
                LevelItem t = a;
                a = b;
                b = t;
            }
        }
    }
}
