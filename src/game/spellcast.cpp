// Casting spells and their effects (0x41b310-0x41d180).
#include "game.h"

static inline float AheadX(int angle, float dist) { return SinDeg(angle) * dist + gPlayer.x; }
static inline float AheadY(int angle, float dist) { return gPlayer.y - CosDeg(angle) * dist; }

// 0x41b310: start casting (spell < 0 opens the spell menu first).
void BeginCast(int spell)
{
    int grp = (spell + 1) / 5;
    int cost = gSpellCost[grp];
    if (gStats.charClass == 1) cost /= 2;
    if (spell < 0) {
        CastDialog();
        spell = gPlayer.currentSpell;
        if (spell < 0) return;
        cost = gSpellCost[grp];
    }
    if (gStats.mana < cost) {
        ShowMessage(gMsg[77] /* You do not have enough spell */, gColorRed);
        ShowMessage(gMsg[78] /* energy to cast that spell */, gColorRed);
        return;
    }
    if (gPlayer.fatigue > 250000) {
        ShowMessage(gMsg[137] /* You are too weak to cast */, gColorRed);
        return;
    }
    if (gSpellsMemorized[spell] <= 0) {
        ShowMessage(gMsg[178] /* No spells memorized */, gColorRed);
        return;
    }

    int active = 0;
    bool check = false;
    switch (spell) {
    case 2: active = gPlayer.identify; check = true; break;
    case 3: case 14: case 24: active = gLightSpell; check = true; break;
    case 7: active = gPlayer.stoneSkin; check = true; break;
    case 9: active = gPlayer.resistUndead; check = true; break;
    case 13: active = gPlayer.magicShield; check = true; break;
    case 21: active = gTimeStop; check = true; break;
    case 26: active = gPlayer.forceField; check = true; break;
    case 27: active = (int)gPlayer.invisibility; check = true; break;
    case 4: case 5: case 10: {
        // aimed at a monster: face it first
        int t = gTarget;
        if (t == -1) {
            t = MonsterUnderMouse();
            gTarget = t;
            if (t == -1) {
                gTargetAngle = gPlayer.angle;
                break;
            }
        }
        int a = AngleToMonster(t);
        gTargetAngle = a;
        int d = abs(a - gPlayer.angle);
        if (d > 0x2d && d < 0x13b) {
            gPlayer.turnAngle = a;
            StartTurn();
            gPlayer.pendingAction = spell + 50;
            return;
        }
        break;
    }
    case 6: case 11: case 15: case 29:
        gTarget = PickTargetScreen();
        if (gTarget == -1) return;
        break;
    case 19: case 28: {
        int a = FindFreeDirection();
        if (a == -1) {
            ShowMessage(gMsg[135] /* No room to summon */, gColorRed);
            return;
        }
        StartLocationEffect(AheadX(a, 1.5f), AheadY(a, 1.5f), spell == 19 ? 0 : 1);
        break;
    }
    }
    if (check && active && !ConfirmDialog()) return;

    static const int kAnim[30] = {4, 7, 7, 7, 3, 3, 8, 8, 8, 8, 5, 9, 9, 9, 9,
                                  10, 10, 10, 10, 10, 7, 11, 11, 4, 11, 8, 12, 12, 12, 12};
    int anim = kAnim[spell];
    gPlayer.actionFrame = anim;
    gPlayer.gameMode = 9;
    gPlayer.castSpell = spell;
    gPlayer.noise += anim * 10;
}

// 0x41b6c0: the casting animation finished; apply the spell.
int CompleteCast(int spell)
{
    int grp = (spell + 1) / 5;
    int cost = gSpellCost[grp];
    if (gPlayer.invisibility) gPlayer.invisibility = 1;
    int roll;
    if (gEquip.armourType == 0)
        roll = rand() % gCastRollNoArmour[gStats.charClass];
    else
        roll = rand() % gCastRollArmour[gStats.charClass];
    if (roll > gStats.maxSpellLevel / 2 + gStats.intel) {
        ShowMessage(gMsg[201] /* Your spell failed... */, gColorRed);
        gHordeCount = 0;
        gLocFxActive = 0;
        return 0;
    }
    switch (spell) {
    case 0: DamageAllMonsters(Dice(0, 1, 3, gStats.intel)); break;
    case 1: HealSpell(5.0f, 1, 2); break;
    case 2: gPlayer.identify = SetEffectTimer(300, 30); break;
    case 3: LightSpell(0); break;
    case 4:
        if (!FireProjectile(AheadX(gPlayer.angle, 1.0f), AheadY(gPlayer.angle, 1.0f), gTargetAngle, 3, 0)) {
            ShowMessage(gMsg[136] /* No room to cast that spell */, gColorRed);
            return 0;
        }
        break;
    case 5:
        if (!FireProjectile(AheadX(gPlayer.angle, 1.0f), AheadY(gPlayer.angle, 1.0f), gTargetAngle, 0, 0)) {
            ShowMessage(gMsg[136] /* No room to cast that spell */, gColorRed);
            return 0;
        }
        break;
    case 6: SlowSpell(); break;
    case 7: gPlayer.stoneSkin = SetEffectTimer(300, 30); break;
    case 8: CureDisease(); break;
    case 9: gPlayer.resistUndead = SetEffectTimer(120, 15); break;
    case 10:
        AimAngle();
        if (!FireProjectile(AheadX(gPlayer.angle, 1.0f), AheadY(gPlayer.angle, 1.0f), gTargetAngle, 1, 0)) {
            ShowMessage(gMsg[136] /* No room to cast that spell */, gColorRed);
            return 0;
        }
        break;
    case 11: FearSpell(); break;
    case 12: RevealSpell(); break;
    case 13: gPlayer.magicShield = SetEffectTimer(60, 12); break;
    case 14: LightSpell(1); break;
    case 15:
        if (!MonsterSpellWeaken(gTarget)) return 0;
        break;
    case 16: HealSpell(7.0f, 2, 4); break;
    case 17: RemoveCurse(); break;
    case 18: KnockSpell(); break;
    case 19: break;
    case 20: MissileVolley(0); break;
    case 21: TimeStopSpell(); break;
    case 22:
        gStats.hp = gStats.maxHp;
        gPlayer.poisoned = 0;
        gPlayer.fatigue -= 75000;
        if (gPlayer.fatigue < 0) gPlayer.fatigue = 0;
        ShowMessage(gMsg[34] /* You are revitalized!! */, gColorGreen);
        break;
    case 23: TeleportSpell(); break;
    case 24: LightSpell(2); break;
    case 25: MissileVolley(1); break;
    case 26:
        gPlayer.forceField = SetEffectTimer(7, 3);
        SetSoundLoop(0x5a, 1);
        PlaySound(0x5a, -1, -1);
        break;
    case 27: gPlayer.invisibility = SetEffectTimer(120, 12); break;
    case 28: gHordeCount = rand() % 3 + 2; break;
    case 29:
        if (!MonsterSpellKill(gTarget)) return 0;
        break;
    }
    if (--gSpellsMemorized[spell] <= 0) gPlayer.currentSpell = -1;
    gStats.mana -= cost;
    GainMagicExperience(gSpellXP[grp]);
    UpdatePoisonDisplay(1);
    return 1;
}

// 0x41bba0: eight missiles, aimed at visible monsters where possible.
void MissileVolley(int kind)
{
    char flags[50];
    CountMonsters(flags, 0xff);
    int m = 0;
    int k = 0;
    for (int n = 8; n; n--) {
        int a = gPlayer.angle + gMissileSpread[k];
        if (m >= 50) {
            k++;
        } else {
            while (m < 50 && !flags[m]) m++;
            if (m < 50) {
                Monster *mon = GetMonster(gCurLevel, m);
                a = AngleBetween(gPlayer.x, gPlayer.y, mon->x, mon->y);
                m++;
            }
        }
        FireProjectile(AheadX(a, 1.0f), AheadY(a, 1.0f), a, kind, 0);
    }
}

// 0x41bc60
void HealSpell(float amount, int lo, int hi)
{
    char buf[32];
    for (int i = 0; i < gStats.maxSpellLevel; i++) amount += (float)(rand() % (hi - lo + 1) + lo);
    if (gStats.charClass == 1) amount *= 1.3f;
    int heal = (int)amount;
    gStats.hp += heal;
    if (gStats.hp > gStats.maxHp) gStats.hp = gStats.maxHp;
    win_itoa(heal, buf, 10);
    strcat(buf, gMsg[32] /* Health Points restored */);
    ShowMessage(buf, gColorWhite);
    HealWounds(10 - (int)((double)gStats.hp / gStats.maxHp * 10.0f));
}

// 0x41bd90
void SlowSpell()
{
    if (gTarget == -1) {
        ShowMessage(gMsg[80] /* No monster to hold */, gColorWhite);
        return;
    }
    int r = rand() % 50;
    int lvl = r - (int)((float)gStats.intel * -2.5f) + 1;
    HoldMonster(gTarget, lvl, SetEffectTimer(30, 3) - gTicks, 1);
}

// 0x41be00
void FearSpell()
{
    if (gTarget == -1) {
        ShowMessage(gMsg[80] /* No monster to hold */, gColorWhite);
        return;
    }
    int r = rand() % 50;
    int lvl = r - (int)((float)gStats.intel * -2.5f) + 1;
    FearMonster(gTarget, lvl, SetEffectTimer(30, 3) - gTicks);
}

// 0x41be70
void TimeStopSpell()
{
    gTimeStop = SetEffectTimer(30, 3) - gTicks;
    int r = rand() % 50;
    int lvl = r - (int)((double)gStats.intel * -2.5) + 1;
    for (int i = 0; i < 50; i++) HoldMonster(i, lvl, gTimeStop, 0);
    ShowMessage(gMsg[275] /* Time stands still... */, gColorWhite);
}

// 0x41bef0: open a locked door in front of the player.
void KnockSpell()
{
    int chance = (int)((double)gStats.intel * 10.0);
    if (gCurLevel == 22) {
        ShowMessage(gMsg[120] /* You cast the spell, but nothing happens */, gColorWhite);
        return;
    }
    int tx = (int)(SinDeg(gPlayer.angle) + gPlayer.x);
    int ty = (int)(gPlayer.y - CosDeg(gPlayer.angle));
    int t = gLevelMap[gMapRow[ty] + tx];
    if (t != 0x1a && t != 0x1b) {
        ShowMessage(gMsg[15] /* No door to unlock.. */, gColorWhite);
        return;
    }
    int d = DoorAt(tx, ty);
    if (d == -1) {
        ShowMessage(gMsg[15] /* No door to unlock.. */, gColorWhite);
        return;
    }
    Door &door = gDoors[d];
    if (door.open != 4) {
        ShowMessage(gMsg[16] /* The door is not locked */, gColorWhite);
        return;
    }
    if (door.lock > 9) {
        ShowMessage(gMsg[63] /* Your unlock spell is deflected */, gColorWhite);
        return;
    }
    if (gStats.charClass == 1) chance += 20;
    if (rand() % 100 + 1 > chance) {
        ShowMessage(gMsg[40] /* Your UNLOCK spell fails !!! */, gColorRed);
        return;
    }
    door.open = 1;
    ShowMessage(gMsg[41] /* Your UNLOCK spell opens the door */, gColorWhite);
    PlaySound(0x2a, -1, -1);
    PlaySound(gDoorSounds[door.kind], -1, -1);
    GainExperience(150);
}

// 0x41c0c0
void RevealSpell()
{
    RevealItems();
    RevealSecretDoors();
    UpdateMonsterVisibility();
    RevealTraps();
    ComputeView(gPlayer.tileX, gPlayer.tileY, 1);
}

// 0x41c0f0: items in view become known.
void RevealItems()
{
    for (int i = 0; i < 1500; i++) {
        LevelItem &it = gLevelItems[i];
        if (it.active != 1 || it.level != gCurLevel + 1) continue;
        if (gMapVis[it.x * 65 + it.y] >= 0xfe) continue;
        if (it.fa == -1) it.fa = 0;
    }
}

// 0x41c150
void RevealSecretDoors()
{
    for (int x = 0; x < gMapWidth; x++) {
        for (int y = 0; y < gMapHeight; y++) {
            if (gMapVis[x * 65 + y] >= 0xfe) continue;
            if (gLevelMap[gMapRow[y] + x] != 3) continue;
            SetMapCell(x, y, 3, 2);
            AddAnimTile(x, y, 0);
            ShowMessage(gMsg[67] /* You have found a teleport */, gColorWhite);
        }
    }
}

// 0x41c1e0: sparkle over traps around the camera.
void RevealTraps()
{
    for (int y = (int)(gCamY - 13.0f); (float)y < gCamY + 13.0f; y++) {
        for (int x = (int)(gCamX - 13.0f); (float)x < gCamX + 13.0f; x++) {
            uint8_t c = gMapVis[x * 65 + y];
            if (c == 9 || c == 0xb) DetectTrap(x, y, 1);
        }
    }
}

// 0x41c2c0
void RemoveCurse()
{
    int found = 0;
    int32_t *slots = (int32_t *)&gEquip;
    for (int i = 0; i < 5; i++) {
        int s = gCurseSlot[i];
        if (slots[s] == gCurseItem[i]) {
            slots[s] = gCurseReplace[i];
            ShowMessage(gMsg[68] /* The curse is removed!! */, gColorRed);
            found = 1;
        }
    }
    RecalcStats();
    if (!found) ShowMessage(gMsg[69] /* No curses to remove */, gColorWhite);
}

// 0x41c340: tick at which a timed effect ends.
int SetEffectTimer(int base, int perLevel)
{
    return (int)((double)(perLevel * gStats.maxSpellLevel + base) * gTickRate) + gTicks;
}

// 0x41c370: wait for a click on a monster.
int PickTargetScreen()
{
    int done = 0;
    uint8_t key;
    RedrawGameScreen(-1);
    gText.SetColor(gColorWhite, 0);
    gText.PrintC(0x181, (char *)gMsg[176] /* Select foe to cast upon */);
    SetCursor(2);
    AddDirtyRect(0, 0, 640, 480);
    do {
        PumpMessages();
        if (MouseLeftClicked()) done = 1;
        if (MouseRightClicked()) done = -1;
        if (KeyPop(&key) && key == 0x1b) done = -1;
        UpdateAndRestore(&gDDW);
    } while (!done);
    SetCursor(0);
    return MonsterUnderMouse();
}

// 0x41c430: lightning arcs between four orbs circling the player.
void DrawForceField()
{
    if (!gPlayer.forceField) return;
    int h[4] = {0, 0x50, 0, 0x50};
    float xs[4], ys[4];
    int a = (int)((unsigned)gFrameCounter % 18) * 20;
    AddOrbiter(a, 0, &xs[0], &ys[0]);
    AddOrbiter(a + 90, 0x50, &xs[1], &ys[1]);
    AddOrbiter(a + 180, 0, &xs[2], &ys[2]);
    AddOrbiter(a + 270, 0x50, &xs[3], &ys[3]);
    for (int i = 1; i <= 4; i++) {
        if (rand() % 10 >= 4) continue;
        int j = i == 4 ? 0 : i;
        Vec3 from = {xs[i - 1], ys[i - 1], (float)h[i - 1]};
        Vec3 to = {xs[j], ys[j], (float)h[j]};
        BoltBetween(&from, &to);
    }
}

// 0x41c560
void AddOrbiter(int angle, int height, float *outX, float *outY)
{
    FGObject o;
    memset(&o, 0, sizeof o);
    o.type = 0;
    o.light = 0x1f;
    o.obj = &gEffectSprites[9];
    o.fx = CosDeg(angle) * 0.5f + gPlayer.x;
    *outX = o.fx;
    o.fy = gPlayer.y - SinDeg(angle) * 0.5f;
    *outY = o.fy;
    if (gMapVis[(int)o.fx * 65 + (int)o.fy] >= 0xff) return;
    o.depth = ScreenDepth(o.fx, o.fy);
    WorldToScreen(o.fx, o.fy, &o.x0, &o.y0);
    o.x0 += 0x20;
    o.y0 -= height;
    AddFGObject(&o);
}

// 0x41c650
void StartLocationEffect(float x, float y, int kind)
{
    gLocFxKind = kind;
    gLocFxSprite = gLocFxFirst[kind];
    gLocFxX = x;
    gLocFxY = y;
    if (kind == 0)
        gLocFxDepth = ScreenDepth(x + 0.5f, y + 0.5f);
    else
        gLocFxDepth = ScreenDepth(x, y);
    gLocFxFrame = 0;
    gLocFxUnused = 0;
}

// 0x41c6e0: summoning circles and other spell effects at a location.
void UpdateLocationEffect()
{
    if (!gLocFxActive) return;
    FGObject o;
    memset(&o, 0, sizeof o);
    int sx, sy;
    o.type = 1;
    WorldToScreen(gLocFxX, gLocFxY, &sx, &sy);
    sx += 0x20;
    int frame = gLocFxFrame;
    int kind = gLocFxKind;
    if (kind == 2 && frame > 4) sy -= 0xf;
    if (frame == 0) gLocFxAngle = gPlayer.angle;
    switch (kind) {
    case 0:
        if (frame == 9) {
            SpawnServant(gLocFxX, gLocFxY);
            frame = gLocFxFrame;
        }
        o.angle = (float)gLocFxAngle;
        o.model = &gSummonModel;
        o.type |= 0x210;
        o.frame = frame < 10 ? frame : 20 - frame;
        gLocFxAngle = (gLocFxAngle + 0xf) % 360;
        break;
    case 1:
        if (gHordeCount && frame == 0xf) {
            SpawnServant(gLocFxX, gLocFxY);
            frame = 0xe;
            gLocFxFrame = 0xe;
            gHordeCount--;
        }
        o.model = &gHordeModel;
        o.angle = (float)(gLocFxAngle + 0x2d);
        o.frame = frame < 0xf ? frame : 0x1e - frame;
        o.type |= 0x210;
        break;
    case 2:
        if (frame == 1) {
            PlaySound(10, (int)gLocFxX, (int)gLocFxY);
            g_5cab64 = 1;
        }
        o.obj = &gEffectSprites[gLocFxSprite];
        break;
    case 3:
        if (frame == 7)
            PlaySound(0x57, (int)gLocFxX, (int)gLocFxY);
        else if (frame == 10)
            DropItem(0x17, 0x16, 0x79, rand() % 15 + 5);
        o.obj = &gEffectSprites[gLocFxSprite];
        break;
    }
    o.x0 = sx;
    o.y0 = sy;
    o.light = 0x1f;
    o.depth = gLocFxDepth;
    AddFGObject(&o);
    if (++gLocFxFrame == gLocFxLen[gLocFxKind])
        gLocFxActive = 0;
    else
        gLocFxSprite++;
}

// 0x41c990
void LightSpell(int kind)
{
    static const int kBase[3] = {300, 480, 900};
    static const int kPerLevel[3] = {30, 30, 45};
    gLightSpell = SetEffectTimer(kBase[kind], kPerLevel[kind]) - gTicks;
    gLightKind = kind;
    gLightFrame = gLightFrameFirst[kind];
    UpdateLightRadius();
}

// 0x41ca10
void DrawLightSpellAt(int sx, int sy)
{
    gLightSx = sx;
    gLightSy = sy - 0x5a;
    DrawLightSpell();
}

// 0x41ca30: the floating light orb above the player.
void DrawLightSpell()
{
    if (!gLightSpell) return;
    int kind = gLightKind;
    int f = ++gLightFrame;
    if (f > gLightFrameLast[kind]) {
        f = gLightFrameFirst[kind];
        gLightFrame = f;
    }
    FGObject o;
    memset(&o, 0, sizeof o);
    o.type = 1;
    o.x0 = gLightSx;
    o.y0 = gLightSy;
    o.obj = &gEffectSprites[f];
    o.depth = gPlayer.depth - -0.1f;
    o.light = 0x1f;
    AddFGObject(&o);
    if (--gLightSpell == 100) {
        ShowMessage(gMsg[92] /* Your LIGHT spell is running low */, gColorWhite);
        return;
    }
    if (gLightSpell == 0) {
        UpdateLightRadius();
        ShowMessage(gMsg[93] /* Your LIGHT spell has expired */, gColorWhite);
    }
}

// 0x41cb10: rings of regeneration (2) and the cursed draining ring (5).
void RingEffects()
{
    if (gEquip.ring1 == 2 || gEquip.ring2 == 2) {
        int t = gTime / 1500;
        if (t > gRegenTime) {
            gRegenTime = t;
            if (gStats.hp < gStats.maxHp) {
                gStats.hp++;
                ShowMessage(gMsg[96] /* Regenerating!! */, gColorAzure);
            }
        }
    }
    if (gEquip.ring1 == 5 || gEquip.ring2 == 5) {
        int t = gTime / 1500;
        if (t > gDrainTime) {
            gDrainTime = t;
            DamagePlayer(1, 0);
            ShowMessage(gMsg[97] /* Degenerating!! */, gColorRed);
        }
    }
}

// 0x41cc10
void CureDisease()
{
    if (gPlayer.diseased) {
        gPlayer.diseaseTime = 0x438;
        ShowMessage(gMsg[114] /* Your disease has been slowed */, gColorWhite);
    } else {
        ShowMessage(gMsg[115] /* You are not sick!! */, gColorWhite);
    }
}

// 0x41cc60: first free floor tile around the player, as an angle.
int FindFreeDirection()
{
    for (int k = 0, a = 0; k < 8; k++, a += 0x2d) {
        int ang = gPlayer.angle + a;
        int tx = (int)(SinDeg(ang) * 1.5f + gPlayer.x);
        int ty = (int)(gPlayer.y - CosDeg(ang) * 1.5f);
        if (gMapVis[tx * 65 + ty] < 0xfe && gLevelMap[gMapRow[ty] + tx] == 0) return gPlayer.angle + a;
    }
    return -1;
}

// 0x41cd00: short range random teleport to an explored tile.
void TeleportSpell()
{
    int done = 0;
    int tries = 0;
    do {
        int tx = rand() % 15 + gPlayer.tileX - 7;
        int ty = rand() % 15 + gPlayer.tileY - 7;
        uint8_t bits = gAutomap[(tx / 8 + gCurLevel * 10) * 65 + ty];
        if ((gBitMask[tx % 8] & bits) && gMapVis[tx * 65 + ty] != 0xff &&
            gLevelMap[gMapRow[ty] + tx] == 0) {
            gPlayer.teleX = tx;
            gPlayer.teleY = ty;
            gPlayer.gameMode = 0xf;
            gPlayer.actionFrame = 0;
            done = 1;
            PlaySound(0x52, gPlayer.tileX, gPlayer.tileY);
        }
        if (++tries == 10 && !done) {
            ShowMessage(gMsg[177] /* Your ESCAPE spell failed!! */, gColorRed);
            done = 1;
        }
    } while (!done);
    g_5c5824 = 1;
}

// 0x41ce30: cancel one of the running spells.
void CancelSpellDialog()
{
    MenuItem items[8];
    for (int i = 0; i < 8; i++) gActionMenu[i].text = gMsg[243 + i];
    int n = 0;
    if (gPlayer.identify) items[n++] = gActionMenu[1];
    if (gLightSpell) items[n++] = gActionMenu[2];
    if (gPlayer.stoneSkin) items[n++] = gActionMenu[3];
    if (gPlayer.magicShield) items[n++] = gActionMenu[4];
    if (gPlayer.forceField) items[n++] = gActionMenu[5];
    if (gPlayer.invisibility) items[n++] = gActionMenu[6];
    if (CountServants()) items[n++] = gActionMenu[7];
    items[n++] = gActionMenu[0];
    for (int i = 0, y = 0xb9; i < n; i++, y += 0x16) {
        MenuItem *m = &items[i];
        m->left = 0x159;
        m->x = 0x159;
        m->top = y;
        m->y = y;
        m->right = gText.StringSize((char *)m->text) + m->x;
        m->bottom = m->y + 0x16;
        m->color = gColorWhite;
        m->hiColor = gColorAzure;
    }
    ShowMouse(0);
    gRunesPCX.Display(0x127, 0x57, gDDW.MakePixel16(0, 0xff, 0));
    gText.Print(0x159, 0x9b, (char *)gMsg[153] /* Select An Option : */, gColorWhite);
    AddDirtyRect(0x127, 0x55, 0x15e, 0x145);
    switch (RunMenu(items, n, gColorRed, nullptr, 0)) {
    case 1: gPlayer.identify = 0; break;
    case 2:
        gLightSpell = 0;
        UpdateLightRadius();
        break;
    case 3:
        gPlayer.stoneSkin = 0;
        LoadPlayerTextures();
        break;
    case 4: gPlayer.magicShield = 0; break;
    case 5:
        gPlayer.forceField = 0;
        StopSound(0x5a);
        break;
    case 6: gPlayer.invisibility = 0; break;
    case 7: DismissServants(1); break;
    }
}

// 0x41d0e0: "cast it again?" yes/no box.
int ConfirmDialog()
{
    Sprite *s = GrabScreen(0xe6, 0xb4, 0xd2, 0x50);
    gText.SetColor(gColorWhite, 0);
    gText.PrintC(0xb4, (char *)gMsg[81] /* That spell is already active. */);
    gText.PrintC(0xc8, (char *)gMsg[198] /* Do you wish to re-cast ? */);
    AddDirtyRect(0, 0, 640, 480);
    int r = YesNoMenu();
    RestoreScreen(0xe6, 0xb4, s);
    return r;
}
