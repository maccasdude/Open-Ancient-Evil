// The shop bell and the shop, the golden statue, parchment scrolls and blood
// decals (0x42cf90-0x42d9b0). The WAV file reader that follows it in the
// original (0x42d9b0-0x42dbe0) belongs to the sound layer (platform/snd.cpp).
#include "game.h"

// 0x42cf90: ring the shop bell.
void RingBell(int vi)
{
    if (!FeatureInReach(vi)) {
        ShowMessage(gMsg[2] /* It is too far away. */, gColorRed);
        return;
    }
    PlaySound(0x43, -1, -1);
    if (!MonsterIsActive(0xb, 0)) {     // the shopkeeper
        ShowMessage(gMsg[138] /* Abdar does not appear to be around */, gColorWhite);
        return;
    }
    ShopScreen();
}

// 0x42d000: with the letter of introduction (item 0x7f) the shopkeeper may
// agree to trade.
void ShopScreen()
{
    if (FindInventoryItem(0x7f) == -1) {
        RunConversation(&gConvHeaders[5]);
        return;
    }
    if (!RunConversation(&gConvHeaders[4])) return;

    PCX pcx;
    int done = 0, sel = -1;
    AddDirtyRect(0, 0, 0x280, 0x1e0);
    UpdateAndRestore(&gDDW);
    gStatusTimer = 0;
    SetStatusY(0x120);
    SetCursor(0);
    if (!pcx.Init(gDDW, (char *)"gamedat\\shop.pcx", 0)) FatalError("cannot load shop PCX");
    auto draw = [&]() {
        pcx.Display(0x39, 0x46, gDDW.MakePixel16(0, 0xff, 0));
        gFaces[11].Draw(0x1cf, 0x77, gDDW);
        DrawShopGrid(sel);
        AddDirtyRect(0x39, 0x46, 0x20d, 0x104);
    };
    draw();
    UpdateAndRestore(&gDDW);
    KeyClear();
    uint8_t key[2] = {0, 0};
    do {
        PumpMessages();
        KeyPop(key);
        if (MouseLeftClicked() || key[0] == ' ') {
            if (MouseInBox(0x20a, 0xfb, 0x11, 0x11)) {
                done++;
            } else {
                if (MouseInBox(0x1d1, 0xd2, 0x48, 0x28)) {
                    sel = BuyItem(sel + gShopScroll);
                } else if (MouseInBox(0x1d4, 0xbc, 0x20, 0x16)) {
                    if (gShopScroll > 0) {
                        gShopScroll--;
                        sel = -1;
                    }
                } else if (MouseInBox(0x1f8, 0xbc, 0x20, 0x16)) {
                    if (gShopScroll < 0x18) {
                        gShopScroll++;
                        sel = -1;
                    }
                } else {
                    sel = ShopSlotAtMouse();
                }
                draw();
            }
        }
        DrawStatusText();
        AddDirtyRect(0, 0x120, 0x280, 0x14);
        UpdateAndRestore(&gDDW);
        if (key[0] == 0x1b) done++;
        key[0] = 0;
    } while (!done);
    ClearStatusText();
    pcx.Release();
}

// 0x42d2f0: buy shop item i. Returns the new selection (relative to the
// scroll position), -1 after a purchase.
int BuyItem(int i)
{
    if (i < 0 || i > 0x23 || gShopItems[i] < 0) return i - gShopScroll;
    int slot = FindFreeInvSlot();
    if (slot == -1) {
        StatusText(gMsg[10] /* Your backpack is already full */);
        return i - gShopScroll;
    }
    int item = gShopItems[i];
    if (gPlayer.gold < (uint32_t)gItemPrice[item]) {
        StatusText(gMsg[98] /* You don't have enough gold */);
        return i - gShopScroll;
    }
    gInventory[slot].item = (int16_t)item;
    float w = CarriedWeight();
    gInventory[slot].item = -1;
    if (!(w <= gStats.maxWeight)) {
        StatusText(gMsg[200] /* Too heavy */);
        return i - gShopScroll;
    }
    gDDW.TintRect(0xb4, 0xbe, 0x118, 0x50, gColorGrey128);
    gText.SetColor(gColorWhite, 0);
    gText.PrintC(0xc8, (char *)gMsg[140] /* Do you wish to buy this item */);
    AddDirtyRect(0xb4, 0xbe, 0x118, 0x50);
    UpdateAndRestore(&gDDW);
    if (!YesNoMenu()) return i - gShopScroll;
    gInventory[slot].item = (int16_t)item;
    gInventory[slot].qty = 0;
    gPlayer.gold -= gItemPrice[item];
    for (int k = i + 1; k < 36; k++) {
        gShopItems[k - 1] = gShopItems[k];
        gShopItems[k] = 0;
    }
    return -1;
}

// 0x42d4a0: shop slot under the mouse; shows the item and its price.
int ShopSlotAtMouse()
{
    char buf[52];
    for (int i = 0; i < 12; i++) {
        if (!MouseInBox(gShopSlotX[i], gShopSlotY[i], 0x38, 0x38)) continue;
        int item = gShopItems[i + gShopScroll];
        if (item <= 0) return -1;
        snprintf(buf, sizeof buf, gMsg[139] /* %s : %d Gold Coins */, gItemNames[gItemTrueNameIdx[item]],
                 gItemPrice[item]);
        buf[0] = (char)toupper(buf[0]);
        StatusText(buf);
        return i;
    }
    return -1;
}

// 0x42d550: the golden statue accepts treasure.
void OfferToStatue(int vi)
{
    if (!FeatureInReach(vi)) {
        ShowMessage(gMsg[2] /* It is too far away. */, gColorWhite);
        return;
    }
    gUIMode = 1;
    SetCursor(0);
    gSavedHudVisible = gHudVisible;
    gHudVisible = 1;
    gViewHeight = 0x18f;
    RedrawGameScreen(-1);
    gText.SetColor(gColorWhite, 0);
    gText.PrintC(0x17c, (char *)gMsg[234] /* Select an item to donate */);
    AddDirtyRect(0, 0, 0x280, 0x1e0);
    UpdateAndRestore(&gDDW);
    int slot = PickInventoryItem();
    if (slot != -1) {
        if (IsTreasure(gInventory[slot].item)) {
            StartLocationEffect(23.0f, 22.0f, 3);
            gLocFxActive = 1;
            gInventory[slot].item = -1;
            g_5c5824 = 1;
        } else {
            ShowMessage(gMsg[152] /* The statue will not accept that */, gColorWhite);
        }
    }
    gHudVisible = gSavedHudVisible;
    if (!gSavedHudVisible) gViewHeight = 0x1df;
    KeyClear();
    ResetMouseClicks();
    ShowMouse(1);
    gUIMode = 0;
}

// 0x42d6b0: click a backpack item (right click or ESC cancels).
int PickInventoryItem()
{
    int done = 0;
    gKeyBuf[0] = 0;
    do {
        int ch = 0;
        gHudFlags = 0;
        PumpMessages();
        if (KeyPop(gKeyBuf)) ch = (int8_t)gKeyBuf[0];
        if (MouseLeftClicked() || ch == ' ') {
            InventoryArrows();
            int s = InventorySlotAtMouse();
            if (s != -1 && gInventory[s].item != -1) return s;
        }
        if (MouseRightClicked() || ch == 0x1b) done = 1;
        DrawHud();
        UpdateAndRestore(&gDDW);
    } while (!done);
    return -1;
}

// 0x42d740
int IsTreasure(int item)
{
    return (item >= 50 && item <= 61) || (item >= 70 && item <= 77) || item == 126;
}

// 0x42d770: a parchment; qty picks the picture.
void ReadScroll(int slot)
{
    PCX pcx;
    char name[40];
    int done = 0;
    gUIMode = 8;
    RedrawGameScreen(-1);
    snprintf(name, sizeof name, "gamedat\\scroll-%d.pcx", gInventory[slot].qty);
    pcx.Init(gDDW, name, 1);
    // (port: only the black round the parchment is see-through; the black
    // inside the close box and other dark art was keyed out too, showing
    // the status bar through it)
    pcx.KeyOutsideOnly(gColorNearBlack);
    pcx.Display(0x78, 0, gColorNearBlack);
    AddDirtyRect(0, 0, 0x280, 0x1e0);
    ShowMouse(1);
    UpdateAndRestore(&gDDW);
    SetCursor(0);
    do {
        PumpMessages();
        if (MouseLeftClicked() && MouseInBox(0x1c6, 0x1a9, 0x1e, 0x1e)) done++;
        uint8_t key[2];
        if (KeyPop(key) && (key[0] & 0x7f) == 0x1b) done++;
        gDDW.UpdateScreen();
    } while (!done);
    KeyClear();
    ResetMouseClicks();
    gUIMode = 0;
}

// 0x42d8d0: blood on the floor at (x,y); repeated hits make the pool bigger.
void BloodPool(int x, int y, int type)
{
    int32_t &cell = gDecalMap[x * 65 + y];
    int i = cell;
    if (i == -1) {
        for (i = 0; i < 600 && gDecals[i].active; i++) {}
        if (i == 600) return;
    }
    int sprite;
    if (type == 1) {
        sprite = 0x13;
    } else {
        int s = gDecals[i].sprite;
        if (s >= 7 && s <= 0x12) sprite = 0;
        else if (s == 0) sprite = 1;
        else if (s == 1) sprite = 2;
        else return;
    }
    cell = i;
    g_5c5824 = 1;
    Decal &d = gDecals[i];
    d.sprite = (int16_t)sprite;
    d.x = (int16_t)x;
    d.y = (int16_t)y;
    d.level = (int16_t)(gCurLevel + 1);
    int32_t t = gTime;
    memcpy(&d.fa, &t, 4);   // fa/fc hold the time it was made
    d.active = 1;
}
