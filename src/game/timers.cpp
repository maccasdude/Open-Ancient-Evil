// Per-tick timers (effects wearing off, torch, ambient sounds), resting, the
// number dialog, food and other usable items, pits, traps and fire vents
// (0x42b920-0x42cf90).
#include "game.h"

// 0x42b920: called once per game tick.
void UpdateTimers()
{
    if (gAltView) {
        gAltView--;
        gViewOffX = 12 - rand() % 6;
        gViewOffY = 8 - (rand() & 3);
        g_5c5824 = 1;
    }
    if (gPlayer.noise) gPlayer.noise--;
    if (gStats.mana != gStats.maxMana && gTicks % (uint32_t)gManaRegenPeriod[gStats.charClass] == 0 &&
        !(gUIMode == 9 && gCurLevel == 0x16))
        gStats.mana++;
    if (gPlayer.poisoned && gTicks % 540 == 0) {
        DamagePlayer(gPlayer.poisoned, 0);
        ShowMessage(gMsg[45] /* Poisoned!! */, gColorGreen);
    }
    if (gPlayer.diseased && gTicks % (uint32_t)gPlayer.diseaseTime == 0) {
        DamagePlayer(gPlayer.diseased * 2, 0);
        ShowMessage(gMsg[113] /* Diseased!!! */, gColorGreen);
    }
    if (gPlayer.identify && gTicks > (uint32_t)gPlayer.identify) {
        gPlayer.identify = 0;
        ShowMessage(gMsg[35] /* Your IDENTIFY MAGIC spell has expired */, gColorGreen);
    }
    if (gPlayer.stoneSkin && gTicks > (uint32_t)gPlayer.stoneSkin) {
        gPlayer.stoneSkin = 0;
        ShowMessage(gMsg[36] /* Your STONE SKIN spell has expired */, gColorGreen);
        LoadPlayerTextures();
    }
    if (gPlayer.magicShield && gTicks > (uint32_t)gPlayer.magicShield) {
        gPlayer.magicShield = 0;
        ShowMessage(gMsg[37] /* Your SHIFTING IMAGE spell has expired */, gColorGreen);
    }
    if (gPlayer.invisibility && gTicks > gPlayer.invisibility) {
        gPlayer.invisibility = 0;
        ShowMessage(gMsg[38] /* Your INVISIBILITY spell has expired */, gColorGreen);
    }
    if (gPlayer.forceField && gTicks > (uint32_t)gPlayer.forceField) {
        gPlayer.forceField = 0;
        ShowMessage(gMsg[42] /* Your MAGIC SHIELD spell has expired */, gColorGreen);
        StopSound(0x5a);
    }
    if (gPlayer.resistUndead && gTicks > (uint32_t)gPlayer.resistUndead) {
        gPlayer.resistUndead = 0;
        ShowMessage(gMsg[90] /* Your RESIST FIRE spell has expired */, gColorGreen);
    }
    if (gEquip.shield == 2) {   // burning torch
        gPlayer.noise++;
        if (gTorchFuel <= 0) {
            if (gPlayer.gameMode == 0) {
                gEquip.shield = 3;
                UpdateLightRadius();
                ShowMessage(gMsg[83] /* Your torch is burnt out */, gColorWhite);
                FreePlayerModel();
                LoadPlayerModel();
            }
        } else if (gActionMode == 0) {
            gTorchFuel--;
            if (gTorchFuel < 1000 && gTorchFuel % 250 == 0)
                ShowMessage(gMsg[82] /* Your torch is running low */, gColorWhite);
        }
    }
    if (--gAmbientTimer <= 0) {
        int y = rand() % 65;
        int x = rand() % 65;
        PlaySound(0x40, x, y);
        gAmbientTimer = rand() % 150 + 270;
    }
    if (gTimeStop && --gTimeStop == 0)
        ShowMessage(gMsg[224] /* Your HOLD TIME spell has expired */, gColorWhite);
}

// 0x42bc90
int FindInventoryItem(int item)
{
    for (int i = 0; i < 30; i++)
        if (gInventory[i].item == item) return i;
    return -1;
}

// ---- resting ------------------------------------------------------------------

// 0x42bcc0: the bedroll (and beds).
void Rest()
{
    char flags[52];
    int t = gLevelMap[gMapRow[gPlayer.tileY] + gPlayer.tileX];
    if (CountMonsters(flags, 0)) {
        ShowMessage(gMsg[86] /* Monsters in range !! */, gColorWhite);
        return;
    }
    if (t == 6 || t == 7 || t == 9) {
        ShowMessage(gMsg[110] /* You cannot sleep here */, gColorWhite);
        return;
    }
    if (gStats.hp == gStats.maxHp && gStats.mana == gStats.maxMana) {
        ShowMessage(gMsg[263] /* You are not fatigued or injured */, gColorWhite);
        return;
    }
    int hours = RestDialog();
    if (!hours) return;
    SleepHours(hours);
    DismissServants(1);
    UpdateLightRadius();
    DrawEffectIcons();
    gDDW.Clear(0);
    gDDW.UpdateScreen();
}

// 0x42bdd0: confirm when poisoned or diseased, then ask for how long.
int RestDialog()
{
    RedrawGameScreen(-1);
    AddDirtyRect(0, 0, 0x280, 0x1e0);
    SetCursor(0);
    if (gPlayer.poisoned || gPlayer.diseased) {
        gText.PrintC(0xb4, (char *)(gPlayer.poisoned ? gMsg[271] /* You are poisoned!! */
                                                      : gMsg[272] /* You are diseased!! */), gColorWhite);
        gText.PrintC(0xc8, (char *)gMsg[273] /* Are you sure you wish to rest ? */, gColorWhite);
        AddDirtyRect(0, 0xb4, 0x280, 0x28);
        if (!YesNoMenu()) {
            gUIMode = 0;
            return 0;
        }
    }
    int hours = VolumeDialog(0, 12, (char *)"gamedat\\sleep.pcx", gMsg[87] /* How long will you rest for ? */, 0);
    gDDW.FillRect(0, 0, 0x27f, 0x1df, 0);
    gUIMode = 0;
    return hours;
}

// 0x42bee0: an hour is 3600 ticks; warriors heal faster. Resting on level 22
// does not heal.
void SleepHours(int hours)
{
    char buf[24];
    int heal = 2, done = 0;
    ShowMouse(0);
    if (gLightSpell) {
        gLightSpell = 0;
        ShowMessage(gMsg[93] /* Your LIGHT spell has expired */, gColorWhite);
    }
    if (gTimeStop) gTimeStop = 0;
    gUIMode = 9;
    if (gStats.charClass == 0) heal = 3;
    PlaySound(0x36, -1, -1);
    int hour = 1;
    do {
        for (int i = 0; i < 3600; i++) {
            gTicks++;
            gPlayer.fatigue += 4;
            UpdateTimers();
        }
        if (gCurLevel != 0x16) {
            gStats.hp += heal;
            if (gStats.hp > gStats.maxHp) gStats.hp = gStats.maxHp;
            gStats.mana += 15;
            if (gStats.mana > gStats.maxMana) gStats.mana = gStats.maxMana;
        }
        gDDW.FillRect((RECT *)nullptr, 0);
        gText.SetColor(gColorWhite, 0);
        gText.PrintC(0xe6, (char *)gMsg[88] /* SLEEPING.. */);
        snprintf(buf, sizeof buf, "%d %s", hour, gMsg[89] /*  HOURS */);
        gText.PrintC(0xfa, buf);
        DrawMessages();
        gDDW.UpdateScreen();
        WaitTicks(20);
        hour++;
        if (hour > hours || (gStats.hp == gStats.maxHp && gStats.mana == gStats.maxMana)) done = 1;
    } while (!done);
    WaitSound(0x36);
    if (gCurLevel == 0x16) ShowMessage(gMsg[121] /* You do not feel rested */, gColorWhite);
    HealWounds(10 - (int)((double)gStats.hp / gStats.maxHp * 10.0));
    gPlayer.actionFrame = 0;
    gPlayer.gameMode = 0;
    FreePlayerModel();
    LoadPlayerModel();
    ShowMouse(1);
    gUIMode = 0;
}

// 0x42c110: pick a number between min and max with the arrows. Returns cur
// unchanged on escape, -1 if the picture is missing.
int VolumeDialog(int min, int max, char *pcxName, const char *title, int cur)
{
    PCX pcx;
    char buf[16];
    int done = 0, v = cur;
    SetCursor(0);
    if (!pcx.Init(gDDW, pcxName, 0)) return -1;
    auto draw = [&]() {
        pcx.Display(0xb2, 0x77, gDDW.MakePixel16(0, 0xff, 0));
        gText.SetColor(gColorWhite, 0);
        gText.PrintC(0xa1, (char *)title);
        sprintf(buf, "%d", v);
        gText.PrintC(0xc0, buf);
        AddDirtyRect(0xaf, 0x73, 0x122, 0xaa);
    };
    draw();
    do {
        PumpMessages();
        int cmd = 0;
        uint8_t key[2];
        if (KeyPop(key)) {
            if (key[0] == 0x1b) cmd = 1;
            else if (key[0] == 0xd) cmd = 2;
            else if (key[1] == 0x25) cmd = 3;
            else if (key[1] == 0x27) cmd = 4;
        } else if (MouseLeftClicked()) {
            if (MouseInBox(0x1a5, 0xf5, 0xf, 0xf)) cmd = 1;
            else if (MouseInBox(0x11c, 0xdb, 0x4a, 0x28)) cmd = 2;
            else if (MouseInBox(0x104, 0xbd, 0x22, 0x14)) cmd = 3;
            else if (MouseInBox(0x15c, 0xbd, 0x22, 0x14)) cmd = 4;
        }
        if (cmd) {
            switch (cmd) {
            case 1: v = cur; done = 1; break;
            case 2: done = 1; break;
            case 3: if (--v < min) v = min; break;
            case 4: if (++v > max) v = max; break;
            }
            draw();
        }
        UpdateAndRestore(&gDDW);
    } while (!done);
    pcx.Release();
    gUIMode = 0;
    return v;
}

// ---- food and other items -------------------------------------------------------

// 0x42c460: nightshade restores spell points; the stalk is left over.
void EatNightshade(int slot)
{
    char buf[40];
    int n = (rand() & 7) + 2;
    gStats.mana += n;
    if (gStats.mana > gStats.maxMana) gStats.mana = gStats.maxMana;
    snprintf(buf, sizeof buf, "%d%s", n, gMsg[91] /*  Spell Points restored */);
    ShowMessage(buf, gColorAzure);
    UpdatePoisonDisplay(1);
    gInventory[slot].item = 0x85;
}

// 0x42c510: the stalk is poisonous.
void EatNightshadeStalk(int slot)
{
    gPlayer.poisoned = 1;
    ShowMessage(gMsg[46] /* You have been poisoned!! */, gColorGreen);
    UpdatePoisonDisplay(1);
    gInventory[slot].item = -1;
}

// 0x42c550: food lowers the hunger counter (Player.fatigue).
void EatFood(int slot)
{
    if (gInventory[slot].item == 100 && gPlayer.fatigue < 250000) {   // a severed arm
        ShowMessage(gMsg[270] /* You are not that hungry...yet */, gColorRed);
        return;
    }
    if (gPlayer.fatigue <= 35000) {
        ShowMessage(gMsg[105] /* You are already full */, gColorWhite);
        return;
    }
    gPlayer.fatigue -= 50000;
    if (gPlayer.fatigue < 0) gPlayer.fatigue = 0;
    gInventory[slot].item = -1;
    gStats.hp += rand() % 5;
    if (gStats.hp > gStats.maxHp) gStats.hp = gStats.maxHp;
    ShowMessage(gMsg[3] /* O.K. */, gColorWhite);
    PlaySound(0x35, -1, -1);
}

// 0x42c620: falling into a pit (tile 1) kills unless a ring of levitation
// (ring 4) is worn. Returns 1 if the player fell.
int CheckPit()
{
    if (gPlayer.gameMode == 5) return 0;
    if (gLevelMap[gMapRow[gPlayer.tileY] + gPlayer.tileX] != 1) return 0;
    if (gEquip.ring1 == 4 || gEquip.ring2 == 4) return 0;
    PlayFallingMovie();
    gStats.hp = -1;
    gDDW.Clear(0);
    return 1;
}

// 0x42c690
void AddAnimTile(int x, int y, int kind)
{
    for (int i = 0; i < 400; i++) {
        LevelAux &a = gLevelAux[i];
        if (a.level != -1) continue;
        a.level = (int16_t)gCurLevel;
        a.x = (int16_t)x;
        a.y = (int16_t)y;
        a.kind = (int16_t)kind;
        InitAnimTiles();
        return;
    }
}

// 0x42c6f0: a ring of life (3) saves the player once and turns into ring 6.
void RingOfLife()
{
    if (gEquip.ring1 == 3) gEquip.ring1 = 6;
    else if (gEquip.ring2 == 3) gEquip.ring2 = 6;
    else return;
    gStats.hp = gStats.maxHp / 2;
    ShowMessage(gMsg[21] /* Back from the brink of death... */, gColorWhite);
    PlaySound(9, -1, -1);
}

// 0x42c760: the Orb of Transformation holds spell points (qty).
void UseOrb(int slot)
{
    char buf[80];
    int need = gStats.maxMana - gStats.mana;
    if (!need) return;
    int16_t &charge = gInventory[slot].qty;
    if (charge <= 0) {
        ShowMessage(gMsg[117] /* The ORB has expired */, gColorWhite);
        return;
    }
    if (need > charge) need = charge;
    charge = (int16_t)(charge - need);
    gStats.mana += need;
    snprintf(buf, sizeof buf, "%d %s", need, gMsg[116] /* spell energy points restored */);
    ShowMessage(buf, gColorBlue);
    if (charge < 0x32) ShowMessage(gMsg[199] /* Orb power is running low */, gColorRed);
    UpdatePoisonDisplay(1);
    gFlashColor = gColorWhite;
}

// 0x42c840
void PlayFlute()
{
    StopCDMusic();
    gPlayer.gameMode = 7;
    gPlayer.actionFrame = 0;
    FreePlayerModel();
    LoadPlayerModel();
    gPlayerModel = gPlayerAnim.Loop(0);
    ShowMessage(gMsg[119] /* Click the LEFT button to stop playing */, gColorWhite);
    gPlayer.restTime = 0;
}

// 0x42c8a0: the healing fountain (object kind 0x17).
void DrinkFountain(int obj)
{
    (void)obj;
    int x = gPlayer.tileX, y = gPlayer.tileY;
    if (x < 0x1c || x > 0x1e || y < 0x2f || y > 0x31) {
        ShowMessage(gMsg[2] /* It is too far away. */, gColorWhite);
        return;
    }
    if (gPlayer.diseased) {
        gPlayer.diseased = 0;
        ShowMessage(gMsg[122] /* You drink from the fountain.. */, gColorAzure);
        ShowMessage(gMsg[123] /* and your disease is cured!! */, gColorAzure);
    } else if (gStats.hp < gStats.maxHp) {
        gStats.hp++;
        ShowMessage(gMsg[122] /* You drink from the fountain.. */, gColorAzure);
        ShowMessage(gMsg[124] /* and you are healed!! */, gColorAzure);
    } else {
        ShowMessage(gMsg[233] /* You do not need healing */, gColorWhite);
    }
    UpdatePoisonDisplay(1);
}

// ---- traps ----------------------------------------------------------------------

// 0x42c990
void TrapGoesOff(int x, int y)
{
    if (gTrapActive) return;
    gTrapActive = 1;
    gTrapFrame = 0;
    gTrapX = (float)((double)x + 0.5);
    gTrapY = (float)((double)y + 0.5);
    SetMapCell(x, y, 3, 0);
    uint8_t ov = gTrapOverlay[gCurLevel];
    if (ov) SetMapCell(x, y, 1, ov);
    g_5c5824 = 1;
    ShowMessage(gMsg[52] /* You have triggered a trap !! */, gColorRed);
    PlaySound(10, x, y);
}

// 0x42ca40: the trap's explosion (8 frames, damage on frame 2).
void UpdateTrap()
{
    if (!gTrapActive) return;
    if (gFrameCounter & 1) gTrapFrame++;
    if (gTrapFrame == 8) gTrapActive = 0;
    if (gTrapFrame == 2) DamagePlayer(rand() % 10 + 10, 1);
    WorldToScreen(gTrapX, gTrapY, &gTrapSx, &gTrapSy);
    FGObject o;
    memset(&o, 0, sizeof o);
    gTrapSx += 0x20;
    o.x0 = gTrapSx;
    o.y0 = gTrapSy;
    o.depth = ScreenDepth(gTrapX, gTrapY);
    o.obj = &gEffectSprites[12 + gTrapFrame];
    o.light = 0x1f;
    AddFGObject(&o);
}

// 0x42cb20: 'Demon Crusher' on the statue square of level 24. The five
// statue items must be destroyed in order.
void UseDemonCrusher()
{
    int tx = (int)(SinDeg(gPlayer.angle) + gPlayer.x);
    int ty = (int)(gPlayer.y - CosDeg(gPlayer.angle));
    if ((gCurLevel == 0x18 || tx == 0x24 || ty == 0xd) && CountItemsAt(0x24, 0xd) == 1) {
        int item = ItemTypeAt(0x24, 0xd);
        if (item != gStatueItems[gStatuesDestroyed]) {
            ShowMessage(gMsg[133] /* Nothing happens */, gColorRed);
            return;
        }
        PlaySmashMovie();
        RemoveItemsOfType(item);
        if (++gStatuesDestroyed == 5) PlaceLevel24Guards();
        ShowMessage(gMsg[232] /* The Demon Statue has been destroyed */, gColorWhite);
        g_5c5824 = 1;
        return;
    }
    ShowMessage(gMsg[132] /* You cannot use that now */, gColorRed);
}

// 0x42cc20
int CountItemsAt(int x, int y)
{
    int n = 0;
    for (int i = 0; i < 1500; i++) {
        LevelItem &it = gLevelItems[i];
        if (it.active == 1 && it.level == gCurLevel + 1 && it.x == x && it.y == y) n++;
    }
    return n;
}

// 0x42cc80
int ItemTypeAt(int x, int y)
{
    for (int i = 0; i < 1500; i++) {
        LevelItem &it = gLevelItems[i];
        if (it.active == 1 && it.level == gCurLevel + 1 && it.x == x && it.y == y) return it.type;
    }
    return 0;
}

// 0x42cce0: on every level.
void RemoveItemsOfType(int type)
{
    for (int i = 0; i < 1500; i++)
        if (gLevelItems[i].type == type) gLevelItems[i].active = 0;
}

// 0x42cd10: 'The Book of the Undead'
void ReadDemonLore()
{
    char flags[52];
    if (CountMonsters(flags, 0)) {
        ShowMessage(gMsg[86] /* Monsters in range !! */, gColorWhite);
        return;
    }
    ReadBook("GAMeDAT\\demonlor.bok", 2);
}

// ---- fire vents -------------------------------------------------------------------

// 0x42cd60
void AddBurner(int x, int y)
{
    for (int i = 0; i < 4; i++) {
        Burner &b = gBurners[i];
        if (b.active == 1) continue;
        b.x = x;
        b.y = y;
        b.state = 0;
        b.f18 = 0;
        b.frame = 0;
        b.active = 1;
        b.kind = rand() & 1;
        b.sprite = b.kind * 5;
        return;
    }
}

// 0x42cee0
static void DrawBurner(Burner &b)
{
    FGObject o;
    memset(&o, 0, sizeof o);
    WorldToScreen((float)b.x, (float)b.y, &o.x0, &o.y0);
    o.obj = &gArmsSprites[b.sprite];
    o.depth = ScreenDepth((float)((double)b.x + 0.5), (float)((double)b.y + 0.5));
    o.type = 0;
    o.light = gLightMap[b.x * 65 + b.y];
    if (gColorLight) o.lrgb = gLightRGB[b.x * 65 + b.y];
    AddFGObject(&o);
}

// 0x42cdd0: flames ignite, burn while the player stands on the vent and
// die down when he leaves.
void UpdateBurners()
{
    for (int i = 0; i < 4; i++) {
        Burner &b = gBurners[i];
        if (!b.active) continue;
        b.f18 = 0;
        switch (b.state) {
        case 0:
            b.sprite++;
            if (++b.frame == 5) {
                b.state = 1;
                b.frame = 0;
            }
            break;
        case 1:
            b.sprite++;
            if (++b.frame == 5) {
                b.sprite -= 5;
                b.frame = 0;
            }
            if (gPlayer.tileX != b.x || gPlayer.tileY != b.y) {
                b.state = 2;
                b.frame = 0;
                b.sprite = b.kind * 15 + 10;
            }
            break;
        case 2:
            b.sprite++;
            if (++b.frame == 5) b.active = 0;
            break;
        }
        if (b.state == 1 && gPlayer.gameMode != 5 && rand() % 100 + 1 > gStats.tou * 5) {
            DamagePlayer(1, 0);
            UpdatePoisonDisplay(0);
        }
        DrawBurner(b);
    }
}
