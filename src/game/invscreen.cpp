// Mouse / gamma dialogs, the equipment screen with its status line, and using
// items from the backpack: potions, keys, lock picks, the torch
// (0x429550-0x42a7c0).
#include "game.h"

static inline int32_t &EquipSlot(int i) { return ((int32_t *)&gEquip)[i]; }

// 0x429550
void MouseSpeedDialog()
{
    gUIMode = 0;
    SetCursor(0);
    CompactInventory();
    RedrawGameScreen(3);
    AddDirtyRect(0, 0, 0x280, 0x1e0);
    gPrefs.mouseSpeed = (int16_t)VolumeDialog(1, 5, (char *)"gamedat\\m-sens.pcx", gMsg[277] /* Select mouse speed */, gPrefs.mouseSpeed);
    SetMouseSpeed(gPrefs.mouseSpeed);
}

// 0x4295c0
void GammaDialog()
{
    char buf[80];
    SetCursor(0);
    gPrefs.gamma = VolumeDialog(1, 8, (char *)"gamedat\\gamma.pcx", gMsg[281] /* Select gamma level */, gPrefs.gamma);
    gShade.SetGammaLevel((float)gPrefs.gamma);
    ClearFloorCache();
    sprintf(buf, "%s%d", gMsg[222] /* Gamma correction level :  */, gPrefs.gamma);
    ShowMessage(buf, gColorWhite);
}

// 0x429650: the equipment screen.
void InventoryScreen()
{
    gSavedHudVisible = gHudVisible;
    gUIMode = 5;
    gHudVisible = 1;
    gViewHeight = 0x18f;
    ShowMouse(0);
    SetCursor(0);
    int n1 = LoadCSpriteTable((char *)"gamedat\\witems.cst", gEquipSprites, 100);
    int n2 = LoadCSpriteTable((char *)"gamedat\\runes.cst", gRuneSprites, 30);
    DrawInventoryScreen();
    InventoryLoop();
    CompactInventory();
    ReleaseCSpriteTable(gEquipSprites, n1);
    ReleaseCSpriteTable(gRuneSprites, n2);
    FreePlayerModel();
    LoadPlayerModel();
    gPlayerModel = gPlayerAnim.Once(0x12, &gPlayerModel, 1);
    UpdateWalkSpeed();
    gDDW.FillRect(0, 0, 0x27f, 0x1df, 0);
    ShowMouse(1);
    gHudVisible = gSavedHudVisible;
    if (!gSavedHudVisible) gViewHeight = 0x1df;
    gUIMode = 0;
}

// 0x429750
void DrawInventoryScreen()
{
    ShowMouse(0);
    RedrawGameScreen(-1);
    gCharPCX.Display(0x72, 0, gDDW.MakePixel16(0, 0xff, 0));
    DrawCharacterSheet();
    DrawRunes(0x12f, 0x149);
    DrawEquipment();
    ShowMouse(1);
    AddDirtyRect(0, 0, 0x280, 0x1e0);
    UpdateAndRestore(&gDDW);
}

static const char *TrueItemName(int item) { return gItemNames[gItemTrueNameIdx[item]]; }

// 0x4297e0
void InventoryLoop()
{
    int done = 0;
    uint8_t key[2] = {0, 0};
    SetStatusY(0x16a);
    Sprite *save = GrabScreen(0x8a, 0x168, 0x1a6, 0x12);
    ShowMouse(1);
    do {
        int ch = 0;
        PumpMessages();
        gHudFlags = 0;
        if (KeyPop(key)) ch = (int8_t)key[0];
        if (MouseRightClicked()) {
            int s = InventorySlotAtMouse();
            if (s != -1 && gInventory[s].item != -1) {
                int item = gInventory[s].item;
                StatusText(gPlayer.identify ? TrueItemName(item) : gItemNames[gItemNameIdx[item]]);
            } else {
                int e = EquipSlotAtMouse();
                if (e != -1) StatusText(TrueItemName(gEquipItemId[e * 20 + EquipSlot(e)]));
            }
        }
        if (MouseLeftClicked()) {
            InventoryArrows();
            int s = InventorySlotAtMouse();
            if (s != -1) {
                if (gInventory[s].item != -1) EquipItem(s);
            } else {
                int e = EquipSlotAtMouse();
                if (e != -1) UnequipItem(e);
                else if (MouseInBox(0x1e5, 0x114, 0x10, 0x10) || MouseInBox(0x17a, 0x1a1, 0x39, 0x28) ||
                         MouseInBox(0x1d9, 0x1bb, 0x20, 0x20))
                    done = 1;
            }
            CompactInventory();
        }
        if (key[1] == 0x25) ScrollInventory(-1);
        else if (key[1] == 0x27) ScrollInventory(1);
        if (ch == 0x1b) done = 1;
        save->Blt(gDDW, 0x8a, 0x168);
        DrawStatusText();
        AddDirtyRect(0x8a, 0x168, 0x1d0, 0x14);
        DrawHud();
        AddDirtyRect(0, 0x190, 0x280, 0x50);
        UpdateAndRestore(&gDDW);
        key[0] = 0;
    } while (!done);
    KeyClear();
    UpdateLightRadius();
    RestoreScreen(0x8a, 0x168, save);
    ClearStatusText();
}

// 0x429a20
int EquipSlotAtMouse()
{
    for (int i = 0; i < 9; i++)
        if (MouseInRect((RECT *)gEquipRects[i]) && EquipSlot(i)) return i;
    return -1;
}

// 0x429c10: which equipment slot an item goes in, and its index there (1-based).
static int FindEquipSlot(int item, int *slot, int *idx)
{
    for (int s = 0; s < 9; s++) {
        for (int i = 0; i < 20; i++) {
            if (gEquipItemId[s * 20 + 1 + i] == item) {
                *slot = s;
                *idx = i + 1;
                return 1;
            }
        }
    }
    return -1;
}

// 0x429a60: click on a backpack item in the equipment screen.
void EquipItem(int inv)
{
    int item = gInventory[inv].item;
    int slot, idx;
    if (FindEquipSlot(item, &slot, &idx) == -1) {
        switch (item) {
        case 107: UseBandages(inv); break;
        case 109: DrinkAntidote(inv); break;
        case 132: EatNightshade(inv); break;
        case 133: EatNightshadeStalk(inv); break;
        case 147: DrinkElixir(inv); break;
        default: StatusText(gMsg[43] /* You cannot use/wear that item */); break;
        }
        return;
    }
    if (EquipSlot(slot)) {
        if (slot != 1 || gEquip.ring2) {
            StatusText(gMsg[44] /* There is already an item there */);
            return;
        }
        slot = 2;
    }
    if (slot == 4 && gEquip.weaponType == 4) {
        StatusText(gMsg[94] /* You need both hand for a crossbow */);
        return;
    }
    if (item == 0x35 && gEquip.shield) {
        StatusText(gMsg[94] /* You need both hand for a crossbow */);
        return;
    }
    EquipSlot(slot) = idx;
    if (gInventory[inv].item == 0x81) gTorchFuel = gInventory[inv].qty;
    gInventory[inv].item = -1;
    RecalcStats();
    DrawInventoryScreen();
}

// 0x429c60
void UnequipItem(int slot)
{
    int inv = FindFreeInvSlot();
    if (inv == -1) {
        StatusText(gMsg[10] /* Your backpack is already full */);
        return;
    }
    for (int i = 0; i < 5; i++) {
        if (slot == gCurseSlot[i] && EquipSlot(slot) == gCurseItem[i]) {
            StatusText(gMsg[70] /* That item won't come off */);
            return;
        }
    }
    int item = gEquipItemId[slot * 20 + EquipSlot(slot)];
    gInventory[inv].item = (int16_t)item;
    if ((int16_t)item == 0x81) gInventory[inv].qty = (int16_t)gTorchFuel;
    EquipSlot(slot) = 0;
    RecalcStats();
    DrawInventoryScreen();
}

// 0x429d10: boots of speed.
void UpdateWalkSpeed()
{
    if (gEquip.e0c == 2) {
        gWalkSpeed = 0.18f;
        gRunSpeed = 0.32f;
    } else {
        gWalkSpeed = 0.12f;
        gRunSpeed = 0.25f;
    }
}

// ---- status line ------------------------------------------------------------

// 0x429d50
void StatusText(const char *text)
{
    if (gStatusSave) {
        RestoreScreen(10, gStatusY, gStatusSave);
        gStatusSave = nullptr;
    }
    snprintf(gStatusText, sizeof gStatusText, "%s", text);
    gStatusText[0] = (char)toupper(gStatusText[0]);
    gStatusTimer = 200;
    if (!gStatusSave) gStatusSave = GrabScreen(10, gStatusY, 0x26c, 0x10);
}

// 0x429df0
void SetStatusY(int y) { gStatusY = y; }

// 0x429e00: draw the status line, fading out over its last 15 frames.
int DrawStatusText()
{
    if (!gStatusTimer) return 0;
    gStatusTimer--;
    uint16_t c;
    if (gStatusTimer < 15) {
        uint8_t v = (uint8_t)(gStatusTimer * 15);
        c = gDDW.MakePixel16(v, v, v);
    } else {
        c = gColorWhite;
    }
    gText.SetColor((int16_t)c, 0);
    gText.PrintC(gStatusY, gStatusText);
    if (!gStatusTimer) {
        RestoreScreen(10, gStatusY, gStatusSave);
        gStatusSave = nullptr;
    }
    return 1;
}

// 0x429ea0
void ClearStatusText()
{
    if (!gStatusSave) return;
    RestoreScreen(10, gStatusY, gStatusSave);
    gStatusSave = nullptr;
    gStatusTimer = 0;
}

// 0x429ed0
int InventorySlotAtMouse()
{
    for (int i = 0, x = 0xa4; x < 0x20c; i++, x += 0x48)
        if (MouseInBox(x, 0x19c, 0x40, 0x40)) return gInvScroll + i;
    return -1;
}

// ---- using items --------------------------------------------------------------

// 0x429f10
void UseInventoryItem(int slot)
{
    int item = gInventory[slot].item;
    if (item >= 30 && item < 100) InventoryScreen();
    else if (item >= 100 && item < 150) UseMiscItem(item, slot);
    else if (item >= 150) UseKey(item, slot);
    else ShowMessage(gMsg[6] /* Nothing to Use */, gColorWhite);
}

// 0x429f70
void UseMiscItem(int item, int slot)
{
    switch (item) {
    case 100: case 134: case 135: case 136: EatFood(slot); break;
    case 107: UseBandages(slot); break;
    case 108: ReadDemonLore(); break;
    case 109: DrinkAntidote(slot); break;
    case 113: UseMap(); break;
    case 114: UseOrb(slot); break;
    case 115: case 116: case 117: case 118: case 119:
    case 122: case 123: case 130:
        InventoryScreen();
        break;
    case 126: UseDemonCrusher(); break;
    case 129:   // torch: into the shield hand
        if (gEquip.shield) {
            InventoryScreen();
            break;
        }
        gEquip.shield = 2;
        gTorchFuel = gInventory[slot].qty;
        RemoveInventoryItem(slot);
        CompactInventory();
        FreePlayerModel();
        LoadPlayerModel();
        break;
    case 131: Rest(); break;
    case 132: EatNightshade(slot); break;
    case 133: EatNightshadeStalk(slot); break;
    case 137: PlayFlute(); break;
    case 141: PickLock(slot); break;
    case 142: ReadScroll(slot); break;
    case 147: DrinkElixir(slot); break;
    default: ShowMessage(gMsg[48] /* You don't have a use for that */, gColorRed); break;
    }
}

// Message to the status line in the equipment screen, else to the message list.
static void ItemMessage(const char *text, int color)
{
    if (gUIMode == 5) StatusText(text);
    else ShowMessage(text, color);
}

// 0x42a110
void DrinkAntidote(int slot)
{
    (void)slot;
    if (gPlayer.poisoned) {
        gPlayer.poisoned = 0;
        PlaySound(8, -1, -1);
        UpdatePoisonDisplay(1);
        ItemMessage(gMsg[60] /* The poison has been counteracted !! */, gColorWhite);
    } else if (gUIMode == 5) {
        StatusText(gMsg[60] /* The poison has been counteracted !! */);    // sic
    } else {
        ShowMessage(gMsg[61] /* The potion has no effect */, gColorWhite);
    }
}

// Door next to the player (any of 12 directions) for keys and lock picks.
static int AdjacentLockedDoor(int *door)
{
    for (int a = 0; a < 360; a += 30) {
        int tx = (int)(CosDeg(a) + gPlayer.x);
        int ty = (int)(SinDeg(a) + gPlayer.y);
        int t = gLevelMap[gMapRow[ty] + tx];
        if (t != 0x1b && t != 0x1a) continue;
        int d = DoorAt(tx, ty);
        if (d < 0) break;
        if (gDoors[d].open != 4) {
            ShowMessage(gMsg[16] /* The door is not locked */, gColorWhite);
            return 0;
        }
        *door = d;
        return 1;
    }
    ShowMessage(gMsg[15] /* No door to unlock.. */, gColorWhite);
    return 0;
}

static void DoorUnlocked(Door &d)
{
    PlaySound(gDoorSounds[d.kind], -1, -1);
    if (gPlayer.invisibility && CountMonsters(nullptr, 0xff) > 0) gPlayer.invisibility = gTicks + 2;
    GainExperience(50);
    gPlayer.noise += 50;
}

// 0x42a1a0: keys 150+ open the door whose lock % 10 matches.
void UseKey(int item, int slot)
{
    int d;
    if (!AdjacentLockedDoor(&d)) return;
    Door &door = gDoors[d];
    if (door.lock % 10 != item - 150) {
        ShowMessage(gMsg[17] /* That key does not fit the lock */, gColorWhite);
        return;
    }
    PlaySound(0x2a, -1, -1);
    WaitSound(0x2a);
    door.open = 1;
    RemoveInventoryItem(slot);
    ShowMessage(gMsg[18] /* The key fits.. the door is unlocked */, gColorWhite);
    DoorUnlocked(door);
}

static void HealthFeedback()
{
    HealWounds(10 - (int)((double)gStats.hp / gStats.maxHp * 10.0));
}

// 0x42a340: bandages (rangers heal more).
void UseBandages(int slot)
{
    int range = gStats.charClass == 2 ? 10 : 6;
    if (gStats.hp == gStats.maxHp) {
        ItemMessage(gMsg[252] /* You are already at full health */, gColorAzure);
        return;
    }
    gStats.hp += rand() % range + 1;
    if (gStats.hp > gStats.maxHp) gStats.hp = gStats.maxHp;
    ItemMessage(gMsg[221] /* Ahhh.. That feels better. */, gColorAzure);
    RemoveInventoryItem(slot);
    HealthFeedback();
}

// 0x42a430: hot key for the elixir.
void UseElixir()
{
    for (int i = 0; i < 30; i++) {
        if (gInventory[i].item == 0x93) {
            DrinkElixir(i);
            return;
        }
    }
    ShowMessage(gMsg[261] /* You do not have an Elixar of Health */, gColorRed);
}

// 0x42a470: elixir of health (spellcasters heal more).
void DrinkElixir(int slot)
{
    char buf[52];
    int range = gStats.charClass == 1 ? 20 : 12;
    int base = gStats.charClass == 1 ? 5 : 3;
    if (gStats.hp == gStats.maxHp) {
        ItemMessage(gMsg[252] /* You are already at full health */, gColorAzure);
        return;
    }
    int n = rand() % range + base;
    gStats.hp += n;
    if (gStats.hp > gStats.maxHp) gStats.hp = gStats.maxHp;
    snprintf(buf, sizeof buf, "%d%s ", n, gMsg[32] /*  Health Points restored */);
    ItemMessage(buf, gColorAzure);
    RemoveInventoryItem(slot);
    HealthFeedback();
}

// 0x42a590: lock picks (thieves are better at it).
void PickLock(int slot)
{
    int d;
    if (!AdjacentLockedDoor(&d)) return;
    Door &door = gDoors[d];
    if (door.lock >= 10) {
        ShowMessage(gMsg[59] /* Lock pick attempt failed... */, gColorWhite);
        return;
    }
    int r = rand() % 30;
    if (gStats.charClass == 3) r -= 15;
    if (r > gStats.dex) {
        ShowMessage(gMsg[59] /* Lock pick attempt failed... */, gColorWhite);
        if (r - gStats.dex > 10 && rand() % 10 < 2) {
            ShowMessage(gMsg[145] /* Your lockpicks broke */, gColorWhite);
            gInventory[slot].item = -1;
        }
        return;
    }
    PlaySound(0x2a, -1, -1);
    WaitSound(0x2a);
    ShowMessage(gMsg[151] /* You have picked the lock!! */, gColorWhite);
    door.open = 1;
    DoorUnlocked(door);
}

// 0x42a790
void RemoveInventoryItem(int slot)
{
    if (gInventory[slot].item == 0x71) gHaveMap = 0;
    gInventory[slot].item = -1;
}
