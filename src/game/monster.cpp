// Monsters: level tables, AI and combat (0x403680-0x408250).
#include "game.h"

#define CUR_MONSTER(i) (&gMonsters[gCurLevel * 50 + (i)])
#define TILE(x, y) (gLevelMap[gMapRow[(y)] + (x)])
#define VIS(x, y) (gMapVis[(x) * 65 + (y)])

// 0x403690 (static initialiser)
void StaticInit_Monsters() {
    for (int i = 0; i < 25 * 50; i++) {
        gMonsters[i].anim.Construct();
        gMonsters[i].state = 0x80;
    }
}

// 0x4036d0 (static initialiser)
void StaticInit_MonsterAnims() {
    for (int i = 0; i < 8; i++) gMonsterAnims[i].Construct();
}

// 0x403700 (static initialiser)
void StaticInit_MonsterTextures() {
    for (int i = 0; i < 10; i++) gMonsterTextures[i].Construct();
}

// 0x403720: create every monster of every level from the spawn tables
void InitAllMonsters() {
    Monster *m = gMonsters;
    SpawnRecord *s = gSpawns;
    while (m < gMonsters + 25 * 50) {
        for (int i = 0; i < 50; i++, s++, m++)
            if (s->x) m->Init(s, i);
    }
    ClearMonsterMap();
}

// 0x403760
void KillMonster(int level, int idx) { gMonsters[level * 50 + idx].Kill(); }

// 0x403790: re-link monsters to their spawn records (after loading a game)
void RelinkMonsters() {
    ClearMonsterMap();
    SpawnRecord *s = gSpawns;
    Monster *m = gMonsters;
    while (m < gMonsters + 25 * 50) {
        for (int i = 50; i; i--, s++, m++)
            if (m->x != 0.0f) m->SetSpawn(s);
    }
}

// 0x4037e0
void LoadLevelMonsterModels(int level) {
    gNumMonsterTables = 0;
    Monster *m = &gMonsters[level * 50];
    for (int i = 50; i; i--, m++) m->LoadModel();
}

// 0x403820: monster under the mouse cursor (nearest), -1 if none
int MonsterUnderMouse() {
    int sel = -1;
    float best = 999999.0f;
    for (int i = 0; i < 50; i++) {
        if (CUR_MONSTER(i)->HitTest(gMouseX, gMouseY)) {
            Monster *m = CUR_MONSTER(i);
            float d = ScreenDepth(m->x, m->y);
            if (d < best) {
                best = d;
                sel = i;
            }
        }
    }
    return sel;
}

// 0x4038c0
void UpdateMonsters() {
    RegenerateMonsters();
    gHostileCount = CountMonsters(nullptr, 0xff);
    for (int i = 0; i < 50; i++) CUR_MONSTER(i)->Update();
}

// 0x403910
Monster *GetMonster(int level, int idx) { return &gMonsters[level * 50 + idx]; }

// 0x403940: the player talks to a monster
void TalkToMonster(int idx) {
    Monster *m = CUR_MONSTER(idx);
    int type = m->spawn->type;
    if (type == 0x27) {
        if (gMetJetraal) return;
        if (GetAttitudeOfType(0x27) != 1) return;
        RunConversation(&gConvHeaders[1]);
        gMetJetraal = 1;
        KillMonster(4, 0);
        return;
    }
    if (type == 0xf) {
        if (MonsterIsActive(gCurLevel, idx)) {
            ShopScreen();
            return;
        }
    } else if (type == 9) {
        if (!gMetGremlins) {
            RunConversation(&gConvHeaders[8]);
            gMetGremlins = 1;
            return;
        }
        if (!gGremlinsGotOrb) {
            int slot = FindInventoryItem(0x70);
            if (slot == -1) {
                RunConversation(&gConvHeaders[9]);
                return;
            }
            if (!RunConversation(&gConvHeaders[10])) return;
            RemoveInventoryItem(slot);
            CUR_MONSTER(idx)->AddDrop(0x70, 0);
            gGremlinsGotOrb = 1;
            return;
        }
        RunConversation(&gConvHeaders[11]);
        return;
    }
    ShowMessage(gMsg[220] /* No response. */, gColorWhite);
}

// 0x403aa0
uint8_t Monster::GetAttitude() { return stats ? stats->attitude : 0; }

// 0x403ab0: is a projectile touching any monster?
int ProjectileHitsMonster(Projectile *p) {
    int hit = 0;
    for (int i = 0; i < 50; i++) {
        if (CUR_MONSTER(i)->ProjectileHit(p)) hit = 1;
        if (p->kind == 1 && p->power > 0) hit = 0;
    }
    return hit;
}

// 0x403b10
void MonstersCheckTraps() {
    for (int i = 0; i < 50; i++) CUR_MONSTER(i)->CheckTileTrigger();
}

// 0x403b40
void DrawMonsters() {
    for (int i = 0; i < 50; i++) CUR_MONSTER(i)->Draw();
}

// 0x403b70: count visible live monsters (attitude == type, or any if 0xff)
int CountMonsters(char *flags, int attitude) {
    int n = 0;
    if (flags) memset(flags, 0, 50);
    for (int i = 0; i < 50; i++) {
        if (!CUR_MONSTER(i)->IsVisible()) continue;
        if (attitude == 0xff) {
            if (flags) flags[i] = 1;
            n++;
            continue;
        }
        MonsterStats *st = CUR_MONSTER(i)->stats;
        uint8_t a = st ? st->attitude : 0;
        if (attitude == a) {
            if (flags) flags[i] = 1;
            n++;
        }
    }
    return n;
}

// 0x403c30: find a monster in front of the player to fight; turns the player to face it
int FindFightTarget() {
    int base = gPlayer.angle;
    int offs[9] = {0, 15, -15, 30, -30, 45, -45, 60, -60};
    int i = 0;
    for (;;) {
        int a = offs[i] + base;
        float x = SinDeg(a) * 0.7f + gPlayer.x;
        float y = gPlayer.y - CosDeg(a) * 0.7f;
        uint8_t id = MonsterAtPoint(x, y, 0.0f);
        if (id != 0xff) {
            Monster *m = GetMonster(gCurLevel, id);
            gPlayer.angle = AngleBetween(gPlayer.x, gPlayer.y, m->x, m->y);
            return m->stats->b18;
        }
        i++;
        if (i == 9) return 0;
    }
}

// 0x403d40: melee strike in the facing direction
void PlayerStrike() {
    int a = gPlayer.angle;
    float x = SinDeg(a) * 0.7f + gPlayer.x;
    float y = gPlayer.y - CosDeg(a) * 0.7f;
    uint8_t id = MonsterAtPoint(x, y, 0.0f);
    if (id != 0xff) {
        GetMonster(gCurLevel, id)->TakeHit();
        return;
    }
    PlaySound(2, -1, -1);
}

// 0x403dd0: shield bash / kick at a point
int StrikeAt(float x, float y, int angle) {
    float px = SinDeg(angle) * 0.5f + x;
    float py = y - CosDeg(angle) * 0.5f;
    uint8_t id = MonsterAtPoint(px, py, 0.0f);
    if (id != 0xff && CUR_MONSTER(id)->ShieldHit()) return 1;
    PlaySound(2, -1, -1);
    return 0;
}

// 0x403e70: summon a servant (monster type 0x1c) at x,y
void SummonServant(int x, int y) {
    int lvl = gCurLevel;
    Monster *m = &gMonsters[lvl * 50];
    SpawnRecord *s = &gSpawns[lvl * 50];
    int i = 0;
    for (;;) {
        if (m->state == 0x80 && s->type == 0x1c) break;
        i++;
        m++;
        s++;
        if (i == 50) return;
    }
    SpawnRecord *r = &gSpawns[lvl * 50 + i];
    r->x = x;
    r->y = y;
    m->Init(r, i);
    m->LoadModel();
}

// 0x403f10: place Alaric's guards on level 24
void PlaceLevel24Guards() {
    int xs[5] = {0x14, 0xe, 0xe, 0x19, 0x19};
    int ys[5] = {0x1d, 0x20, 0x1f, 0x1f, 0x20};
    SpawnRecord *r = (&gSpawns[24 * 50]);
    for (int i = 0; r < (&gSpawns[24 * 50]) + 5; i++, r++) {
        r->x = xs[i];
        r->y = ys[i];
        GetMonster(0x18, i)->Init(r, i);
    }
}

// 0x403fa0: the priest opens the way
void PriestOpensGate() {
    RunConversation(&gConvHeaders[7]);
    gTalkedToPriest = 1;
    SetAttitudeOfType(0x26, 0);
    for (int i = 0; i < 4; i++) {
        SetMapCell(gGateXs[i], gGateYs[i], 3, 0);
        SetMapCell(gGateXs[i], gGateYs[i], 2, 0xff);
        RemoveWall(gGateXs[i], gGateYs[i]);
    }
}

// 0x404020: spawn the DemonShade where Alaric died
void SpawnDemonShade(float x, float y, int angle) {
    gSpawns[24 * 50 + 5].x = (int)x;
    gSpawns[24 * 50 + 5].y = (int)y;
    gSpawns[24 * 50 + 5].angle = angle;
    Monster *m = GetMonster(0x18, 5);
    m->Init(&gSpawns[24 * 50 + 5], 5);
    m->LoadModel();
    PlaySound(0x63, -1, -1);
}

// 0x404080: expire temporary weapon/armour enchantments
void ExpireEnchantments() {
    if (gEquip.weaponType != 0) {
        if (gEquip.weaponType == 4) return;
        gEquip.weaponType = 0;
        ShowMessage(gMsg[107] /* Your weapon has been dissolved */, gColorRed);
        FreePlayerModel();
        LoadPlayerModel();
        return;
    }
    if (gEquip.ring1) {
        gEquip.ring1 = 0;
        ShowMessage(gMsg[109] /* Your ring has been destroyed */, gColorRed);
        return;
    }
    if (gEquip.ring2) {
        gEquip.ring2 = 0;
        ShowMessage(gMsg[109] /* Your ring has been destroyed */, gColorRed);
        return;
    }
    DamagePlayer(rand() % 5 + 1, 0);
    ShowMessage(gMsg[108] /* Acid burns!! */, gColorWhite);
}

// 0x404140: invisible monsters become visible on lit tiles
void UpdateMonsterVisibility() {
    for (int i = 0; i < 50; i++) {
        Monster *m = CUR_MONSTER(i);
        if (m->invisible && VIS(m->tileX, m->tileY) < 0xfe) m->invisible = 0;
    }
}

// 0x4041a0: load (or share) the model and texture for a monster type
int LoadMonsterModel(MonsterStats *st, AnimSet *dst) {
    char name[80];
    int n = gNumMonsterTables;
    if (n == 8) {
        plat_message_box("Too many monster tables...", "EEK!!");
        return 0;
    }
    int i = 0;
    for (; i < n; i++)
        if (gMonsterTables[i].stats == st) goto found;
    sprintf(name, "GAMEDAT\\MONSTERS\\%s.AMT", st->amtName);
    if (!gMonsterAnims[n].Load(name)) {
        plat_message_box("Monster will not load!!", name);
        return 0;
    }
    gMonsterTables[n].anim = &gMonsterAnims[n];
    sprintf(name, "GAMEDAT\\MONSTERS\\%s.TEX", st->texName);
    if (!gMonsterTextures[n].Load(name)) {
        plat_message_box("Texture will not load!!", name);
        return 0;
    }
    gMonsterTables[n].tex = &gMonsterTextures[n];
    gMonsterAnims[n].SetTexture(&gMonsterTextures[n]);
    gMonsterTables[n].stats = st;
    gNumMonsterTables = n + 1;
found:
    gMonsterTables[i].anim->CopyTo(dst);
    return 1;
}

// 0x404320
int MonsterSpellKill(int idx) {
    if (idx < 0 || idx >= 50) return 0;
    return CUR_MONSTER(idx)->SpellKill();
}

// 0x404360: damage split between all visible monsters
void DamageAllMonsters(int dmg) {
    char flags[52];
    int n = CountMonsters(flags, 0xff);
    if (!n) return;
    int each = dmg / n;
    gFlashColor = gColorWhite;
    if (each > gStats.intel) each = gStats.intel;
    for (int i = 0; i < 50; i++)
        if (flags[i]) CUR_MONSTER(i)->SpellDamage(each);
}

// 0x4043e0
int MonsterSpellWeaken(int idx) {
    if (idx < 0 || idx >= 50) return 0;
    return CUR_MONSTER(idx)->SpellWeaken();
}

// 0x404420: fear spell
void FearMonster(int idx, unsigned level, int time) {
    char buf[80];
    Monster *m = CUR_MONSTER(idx);
    if (m->state != 0x80 && level >= m->stats->fearResist && m->holdTime == 0) {
        m->aiMode = 2;
        m->fearTime = time;
        m->action = 1;
        sprintf(buf, "%s%s", *CUR_MONSTER(idx)->stats->name, gMsg[73] /*  fleas in fear!! */);
        ShowMessage(buf, gColorWhite);
        return;
    }
    ShowMessage(gMsg[72] /* Your spell has no effect */, gColorWhite);
}

// 0x404500: hold spell
void HoldMonster(int idx, unsigned level, int time, int verbose) {
    char buf[80];
    Monster *m = CUR_MONSTER(idx);
    if (m->state != 0x80 && level >= m->stats->holdResist) {
        m->holdTime = time;
        if (!verbose) return;
        sprintf(buf, "%s%s", *CUR_MONSTER(idx)->stats->name, gMsg[71] /*  held!! */);
        ShowMessage(buf, gColorWhite);
        return;
    }
    if (verbose) ShowMessage(gMsg[72] /* Your spell has no effect */, gColorWhite);
}

// 0x4045d0
int MonsterIsActive(int level, int idx) { return gMonsters[level * 50 + idx].IsActive(); }

// 0x404600
void SetAttitudeOfType(int type, int attitude) {
    for (int i = 0; i < 50; i++) {
        Monster *m = CUR_MONSTER(i);
        if (m->type == (uint8_t)type) m->attitude = (uint8_t)attitude;
    }
}

// 0x404650 (searches the current level 25 times, as the original does)
int GetAttitudeOfType(int type) {
    int lvl = gCurLevel;
    for (int k = 0; k < 25; k++) {
        Monster *m = &gMonsters[lvl * 50];
        for (int i = 0; i < 50; i++, m++)
            if (m->type == (uint8_t)type) return m->attitude;
    }
    return 0;
}

// 0x4046a0
int AngleToMonster(int idx) {
    if (idx < 0 || idx >= 50) return 0;
    Monster *m = CUR_MONSTER(idx);
    return AngleBetween(gPlayer.x, gPlayer.y, m->x, m->y);
}

// 0x4046f0: dead regenerating monsters may come back
void RegenerateMonsters() {
    if (gFrameCounter % 30) return;
    for (int i = 0; i < 50; i++) {
        Monster *m = CUR_MONSTER(i);
        if (m->state != 0x80) continue;
        if (m->stats && m->stats->regenerates) m->Regenerate();
    }
}

// 0x404760
void ClearDeadMonsters() {
    for (int i = 0; i < 50; i++) CUR_MONSTER(i)->LeaveCorpse();
}

// 0x404790
void Monster::Init(SpawnRecord *s, int idx) {
    index = (uint8_t)idx;
    state = 0;
    x = (float)s->x - -0.5f;
    destX = x;
    lastSeenX = x;
    y = (float)s->y - -0.5f;
    destY = y;
    lastSeenY = y;
    tileX = (int)x;
    oldX = x;
    tileY = (int)y;
    oldY = y;
    SetSpawn(s);
    MonsterStats *st = stats;
    radius = st->radius;
    hp = st->maxHp;
    attitude = st->attitude;
    action = 1;
    timer = 0;
    aiMode = s->aiMode;
    invisible = st->flags & 0x80;
    angle = s->angle;
    moveAngle = s->angle;
    memcpy(drops, s->drops, sizeof drops);
    chaseDist = 4;
    spawn = s;
    deathTimer = 0;
    xp = st->xp;
}

// 0x4048a0
void Monster::SetSpawn(SpawnRecord *s) {
    spawn = s;
    stats = gMonsterStatsTable[s->type];
    type = s->type;
}

// 0x4048d0
void Monster::LoadModel() {
    if (!stats) return;
    if (LoadMonsterModel(stats, &anim)) model = anim.Loop(0);
}

// 0x404900
void Monster::Update() {
    if (state == 0x80 && !deathTimer) return;
    if (deathTimer) {
        deathTimer--;
        if (deathTimer == 1) {
            LeaveCorpse();
            return;
        }
    }
    UnmarkMonster((int)oldX, (int)oldY, index);
    oldX = x;
    oldY = y;
    if (holdTime) {
        holdTime--;
    } else {
        if (fearTime) {
            fearTime--;
            if (!fearTime) {
                action = 5;
                aiMode = spawn->aiMode;
                timer = (int8_t)(rand() % 5 + 2);
            }
        }
        if (!state) {
            switch (aiMode) {
            case 1: AIStationary(); break;
            case 2: AIFlee(); break;
            case 3: AIWander(); break;
            case 4: AIGuard(); break;
            case 8: AIDeathAnim(); break;
            default: break;
            }
            RegenHp();
        }
    }
    tileX = (int)x;
    tileY = (int)y;
    if (state == 0x80) return;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) MarkMonster(tileX + dx, tileY + dy, index);
}

// 0x404a80
void Monster::Draw() {
    if (state == 0x80 && !deathTimer) return;
    if (invisible && rand() % 100 > gStats.intel) return;
    SetRect(&rect, -2, -2, -1, -1);
    uint16_t light = gLightMap[tileX * 65 + tileY];
    if (!holdTime) frame = anim.Advance();
    if (VIS(tileX, tileY) >= 0xfe) return;
    if (!LineOfSight(gPlayer.x, gPlayer.y, x, y, 0)) return;
    FGObject o;
    memset(&o, 0, sizeof o);
    o.model = model;
    int h = TileHeight(tileX, tileY);
    o.color = h;
    float a = (float)angle - 135.0f;
    if (a < 0.0f) a = a - -360.0f;
    o.angle = a;
    o.fx = x;
    o.frame = frame;
    o.fy = (float)h + y;
    o.light = light;
    if (gColorLight) o.lrgb = SmoothLightModel() ? LightRGBAt(x, y) : gLightRGB[tileX * 65 + tileY];
    o.depth = ScreenDepth(x, y);
    WorldToScreen(x, y, &o.x0, &o.y0);
    o.x0 += 0x1f;
    o.type = 0x10;
    if ((stats->flags & 4) || invisible) o.type = 0x210;
    o.rect = &rect;
    o.scale = stats->scale;
    AddFGObject(&o);
    if (state == 0x80) return;
    if (gPrefs.shadows >= 2) DrawModelShadow(x, y, model, o.frame, angle, 1);
}

// 0x404c60: stays put, turns to face the player, attacks when close
void Monster::AIStationary() {
    if (action == 6) {
        AlertAnim();
    } else if (CanSee(gPlayer.x, gPlayer.y)) {
        if (!attitude) {
            aiMode = 4;
            destX = x;
            destY = y;
            Pursue();
            PlaySound(stats->alertSound, (int)x, (int)y);
            return;
        }
        angle = AngleTo(gPlayer.x, gPlayer.y);
    }
    int r = rand() % 100;
    if (r >= 45) {
        MonsterStats *st = stats;
        if (r < st->idleSoundChance + 45) PlaySound(st->idleSound, (int)x, (int)y);
    }
}

// 0x404d40: random wandering
void Monster::AIWander() {
    if (action != 4 && !TurnToward()) return;
    switch (action) {
    case 1:
        if (tileX != (int)destX || tileY != (int)destY) {
            if (CanMove(angle, stats->moveStep)) {
                MoveToDest();
                return;
            }
        }
        if (rand() % 5) {
            action = 5;
            timer = (int8_t)(rand() % 20);
            model = anim.Loop(0);
        } else {
            moveAngle = rand() % 360;
            float d = (float)(rand() % 3 + 2);
            destX = SinDeg(angle) * d + x;
            destY = y - CosDeg(angle) * d;
        }
        break;
    case 2:
        Walking();
        if (!attitude && CanSee(gPlayer.x, gPlayer.y)) {
            destX = x;
            destY = y;
            aiMode = 4;
            Pursue();
            PlaySound(stats->alertSound, tileX, tileY);
            return;
        }
        break;
    case 4:
        FinishDying();
        break;
    case 5:
        Waiting();
        break;
    default:
        break;
    }
    int r = rand() % 100;
    if (r >= 45) {
        MonsterStats *st = stats;
        if (r < st->idleSoundChance + 45) PlaySound(st->idleSound, (int)x, (int)y);
    }
}

// 0x404f30: turn 'angle' toward 'moveAngle'; returns 1 when facing it
int Monster::TurnToward() {
    while (moveAngle > 360) moveAngle %= 360;
    while (angle > 360) angle %= 360;
    while (moveAngle < 0) moveAngle += 360;
    while (angle < 0) angle += 360;
    int t = moveAngle, a = angle;
    if (t == a) return 1;
    int d = t - a;
    if (abs(d) < 35) {
        angle = t;
        return 1;
    }
    if ((d > 0 && d < 180) || d < -180) angle = (a + 30) % 360;
    else angle = (a + 330) % 360;
    return 0;
}

// 0x405020
void Monster::AIGuard() {
    if (!TurnToward()) return;
    switch (action) {
    case 1: Pursue(); break;
    case 2: Walking(); break;
    case 3: Attacking(); break;
    case 4: FinishDying(); break;
    case 5: Waiting(); break;
    }
}

// 0x405090: choose a pursuit strategy by AI class
void Monster::Pursue() {
    if (aiMode != 4) return;
    MonsterStats *st = stats;
    if (st->ranged) {
        PursueRanged();
        return;
    }
    if (st->aiClass >= 10) PursueClever();
    else if (st->aiClass >= 5) PursueSmart();
    else PursueMelee();
}

// helper: try to walk around an obstacle using the fixed detour table
static int tryDetours(Monster *m, int start) {
    for (int i = 0; i < 5; i++) {
        int idx = start < 0 ? i : (start + i) % 5;
        int a = ((gDetourAngles[idx] + m->angleToPlayer + 45) / 90 % 4) * 90;
        m->moveAngle = a;
        m->destX = SinDeg(a) * (m->stats->moveStep * 5.0f) + m->x;
        m->destY = m->y - CosDeg(m->moveAngle) * (m->stats->moveStep * 5.0f);
        int r = m->MoveToDest();
        if (start < 0 ? r != 0 : r == 1) return 1;
    }
    return 0;
}

// 0x4050d0: dumb melee pursuit
void Monster::PursueMelee() {
    uint8_t sees = (uint8_t)CanSee(gPlayer.x, gPlayer.y);
    savedAngle = moveAngle;
    action = 1;
    float dist = Distance(x, y, gPlayer.x, gPlayer.y);
    angleToPlayer = AngleTo(gPlayer.x, gPlayer.y);
    if (!sees) {
        aiMode = spawn->aiMode;
        return;
    }
    if (ReachedDest()) {
        if (detour) {
            if (!Detour()) goto chase;
            return;
        }
        if (InAttackRange(gPlayer.x, gPlayer.y, angle)) {
            MonsterStats *st = stats;
            aiMode = 4;
            action = 3;
            model = anim.Hold(rand() % st->attackAnimCount + st->attackAnim);
            return;
        }
    } else {
        if (detour) {
            if (!MoveToDest()) return;
        }
    }
chase:
    moveAngle = angleToPlayer;
    {
        int a = (angleToPlayer + 180) % 360;
        destX = SinDeg(a) * radius + gPlayer.x;
        destY = gPlayer.y - CosDeg(a) * radius;
        if (dist <= stats->reach) return;
        if (MoveToDest()) return;
    }
    detour = 1;
    detourAngle = angleToPlayer;
    if (tryDetours(this, -1)) return;
    action = 5;
    timer = (int8_t)(rand() % 5);
    angle = savedAngle;
}

// 0x405330: continue a detour; returns 1 if the monster moved
int Monster::Detour() {
    int d = angleToPlayer - angle;
    if ((d > 0 && d < 180) || d < -180) moveAngle = moveAngle + 90;
    else moveAngle = moveAngle + 270;
    moveAngle %= 360;
    destX = SinDeg(moveAngle) * (stats->moveStep * 5.0f) + x;
    destY = y - CosDeg(moveAngle) * (stats->moveStep * 5.0f);
    int r = MoveToDest();
    if (r == 1) {
        detour = 0;
        return r;
    }
    if (r == -1 && stats->opensDoors) {
        int tx = (int)(SinDeg(moveAngle) + x);
        int ty = (int)(y - CosDeg(moveAngle));
        int di = DoorAt(tx, ty);
        Door *door = &gDoors[di];
        if (!door->open && door->lock != 0x14) {
            door->open = 1;
            PlaySound(gDoorSounds[door->kind], tx, ty);
            action = 5;
            timer = 10;
            detour = 0;
            return 1;
        }
    }
    moveAngle = savedAngle;
    destX = SinDeg(moveAngle) * (stats->moveStep * 5.0f) + x;
    destY = y - CosDeg(moveAngle) * (stats->moveStep * 5.0f);
    return MoveToDest() != 0;
}

// 0x405500: pursuit for moderately clever monsters
void Monster::PursueSmart() {
    uint8_t sees = (uint8_t)CanSee(gPlayer.x, gPlayer.y);
    action = 1;
    float dist = Distance(x, y, gPlayer.x, gPlayer.y);
    angleToPlayer = AngleTo(gPlayer.x, gPlayer.y);
    savedAngle = moveAngle;
    if (sees) {
        lastSeenX = gPlayer.x;
        lastSeenY = gPlayer.y;
    }
    if (InAttackRange(gPlayer.x, gPlayer.y, angle)) {
        MonsterStats *st = stats;
        action = 3;
        model = anim.Once(rand() % st->attackAnimCount + st->attackAnim, &model, 0);
        return;
    }
    if (ReachedDest()) {
        if (!sees) {
            aiMode = spawn->aiMode;
            return;
        }
        if (detour) {
            if (Detour()) return;
            goto detours;
        }
    } else if (detour) {
        if (MoveToDest()) return;
        goto detours;
    }
    if (!sees) {
        destY = lastSeenY;
        destX = lastSeenX;
        if (MoveToDest()) return;
        goto detours;
    }
    if (dist < 1.0) {
        destX = x;
        destY = y;
        moveAngle = angleToPlayer;
        goto detours;
    }
    if (ShouldAttack() == 1) {
        moveAngle = angleToPlayer;
        int a = (angleToPlayer + 180) % 360;
        float d = dist * 0.5f;
        if (d < radius) d = radius;
        if (d > 3.0f) d = 3.0f;
        destX = SinDeg(a) * d + gPlayer.x;
        destY = gPlayer.y - CosDeg(a) * d;
        if (MoveToDest()) return;
        goto detours;
    }
    if (gPlayer.x == lastSeenX && gPlayer.y == lastSeenY) {
        if (rand() % 100 > 30) goto wait;
    }
    {
        int a = (rand() % 45 + angleToPlayer - 22) % 360;
        if (a < 0) a += 360;
        int a2 = a + 180;
        destX = SinDeg(a2) * (float)chaseDist + gPlayer.x;
        destY = gPlayer.y - CosDeg(a2) * (float)chaseDist;
        moveAngle = AngleTo(destX, destY);
        if (MoveToDest()) return;
    }
wait:
    destX = x;
    destY = y;
    action = 5;
    timer = (int8_t)(rand() % stats->aiClass);
    model = anim.Loop(0);
    return;
detours:
    detour = 1;
    detourAngle = angleToPlayer;
    for (int i = 0; i < 5; i++) {
        int a = ((gDetourAngles[i] + angleToPlayer + 45) / 90 % 4) * 90;
        moveAngle = a;
        destX = SinDeg(a) * (stats->moveStep * 5.0f) + x;
        destY = y - CosDeg(moveAngle) * (stats->moveStep * 5.0f);
        if (MoveToDest()) return;
    }
    action = 5;
    timer = (int8_t)(rand() % 5);
    moveAngle = savedAngle;
}

// 0x405940: decide whether to close in for an attack
int Monster::ShouldAttack() {
    if (hp > gStats.hp) return 1;
    if (!gEquip.weaponType) return 1;
    int d = abs(gPlayer.angle - angleToPlayer);
    if (d < 90 || d > 270) return 1;
    MonsterStats *st = stats;
    int cnt = gHostileCount;
    if (st->aiClass >= 10) {
        if (cnt > 3) return 0;
        if (gWeaponDamage[gEquip.weaponType] + gStats.baseStr < st->dmgExtra + st->dmgBase) return 1;
        if (st->weaponLevel) return 1;
    }
    if (cnt < 3) {
        if (rand() % stats->aiClass >= 2) return 0;
    }
    chaseDist--;
    if (chaseDist > 1) return 0;
    chaseDist = 1;
    return 1;
}

// 0x405a20: pursuit for clever monsters
void Monster::PursueClever() {
    uint8_t sees = (uint8_t)CanSee(gPlayer.x, gPlayer.y);
    action = 1;
    float dist = Distance(x, y, gPlayer.x, gPlayer.y);
    angleToPlayer = AngleTo(gPlayer.x, gPlayer.y);
    savedAngle = moveAngle;
    if (sees) {
        lastSeenX = gPlayer.x;
        lastSeenY = gPlayer.y;
    }
    if (InAttackRange(gPlayer.x, gPlayer.y, angle)) {
        MonsterStats *st = stats;
        action = 3;
        model = anim.Once(rand() % st->attackAnimCount + st->attackAnim, &model, 0);
        return;
    }
    if (ReachedDest()) {
        if (!sees) {
            destX = lastSeenX;
            destY = lastSeenY;
            return;
        }
        if (detour) {
            if (Detour()) return;
            goto detours;
        }
    } else if (detour) {
        int r = MoveToDest();
        if (r == -1) {
            BashDoor();
            return;
        }
        if (r) return;
        goto detours;
    }
    if (!sees) {
        destY = lastSeenY;
        destX = lastSeenX;
        int r = MoveToDest();
        if (r == -1) {
            BashDoor();
            return;
        }
        if (r == 1) return;
        goto detours;
    }
    if (dist < 1.0) {
        destX = x;
        destY = y;
        moveAngle = angleToPlayer;
        goto detours;
    }
    if (ShouldAttack() == 1) {
        moveAngle = angleToPlayer;
        int rr = rand() % 180;
        float d = dist * 0.5f;
        int a = (rr + moveAngle + 90) % 360;
        if (d < radius) d = radius;
        if (d > 3.0f) d = 3.0f;
        destX = SinDeg(a) * d + gPlayer.x;
        destY = gPlayer.y - CosDeg(a) * d;
        if (MoveToDest() == 1) return;
        goto detours;
    }
    if (gPlayer.x == lastSeenX && gPlayer.y == lastSeenY) {
        if (rand() % 100 > 30) goto wait;
    }
    {
        int a = (rand() % 45 + angleToPlayer - 22) % 360;
        if (a < 0) a += 360;
        int a2 = a + 180;
        destX = SinDeg(a2) * (float)chaseDist + gPlayer.x;
        destY = gPlayer.y - CosDeg(a2) * (float)chaseDist;
        moveAngle = AngleTo(destX, destY);
        if (MoveToDest() == 1) return;
    }
wait:
    destX = x;
    destY = y;
    action = 5;
    timer = (int8_t)(rand() % stats->aiClass);
    model = anim.Loop(0);
    return;
detours: {
    detour = 1;
    detourAngle = angleToPlayer;
    int r0 = rand() % 5;
    for (int i = 0; i < 5; i++) {
        int a = ((gDetourAngles[(r0 + i) % 5] + angleToPlayer + 45) / 90 % 4) * 90;
        moveAngle = a;
        destX = SinDeg(a) * (stats->moveStep * 5.0f) + x;
        destY = y - CosDeg(moveAngle) * (stats->moveStep * 5.0f);
        if (MoveToDest() == 1) return;
    }
    action = 5;
    timer = (int8_t)(rand() % 5);
    moveAngle = savedAngle;
}
}

// 0x405eb0: open a door in the way
void Monster::BashDoor() {
    int tx = (int)(SinDeg(moveAngle) + x);
    int ty = (int)(y - CosDeg(moveAngle));
    Door *door = &gDoors[DoorAt(tx, ty)];
    if (door->open) return;
    if (door->lock == 0x14) return;
    door->open = 1;
    PlaySound(gDoorSounds[door->kind], tx, ty);
    action = 5;
    timer = 10;
}

// 0x405f40: pursuit for monsters with a ranged attack
void Monster::PursueRanged() {
    uint8_t sees = (uint8_t)CanSee(gPlayer.x, gPlayer.y);
    action = 1;
    float dist = Distance(x, y, gPlayer.x, gPlayer.y);
    angleToPlayer = AngleTo(gPlayer.x, gPlayer.y);
    savedAngle = moveAngle;
    if (sees) {
        lastSeenX = gPlayer.x;
        lastSeenY = gPlayer.y;
    }
    if (InAttackRange(gPlayer.x, gPlayer.y, moveAngle)) {
        action = 3;
        moveAngle = AngleTo(gPlayer.x, gPlayer.y);
        MonsterStats *st = stats;
        model = anim.Once(rand() % st->attackAnimCount + st->attackAnim, &model, st->attackAnim);
        chaseDist = 5;
        return;
    }
    if (rangedTimer) {
        rangedTimer--;
    } else if (sees && dist > 3.0f) {
        if (rand() % 40 >= stats->aiClass) return;
        moveAngle = AngleTo(gPlayer.x, gPlayer.y);
        int spread = 10 - rand() % stats->dmgRange;
        if (rand() % 2) spread = -spread;
        int a = (spread + moveAngle) % 360;
        if (a < 0) a += 360;
        int servant = spawn->type == 0x1b;
        if (!FacingPlayer()) return;
        MonsterStats *st = stats;
        float fy = y - CosDeg(angle) * st->reach;
        float fx = SinDeg(angle) * stats->reach + x;
        if (!FireProjectile(fx, fy, a, st->ranged, servant)) goto detours;
        rangedTimer = stats->rangedDelay;
        model = anim.Once(stats->rangedAnim, &model, 0);
        return;
    }
    if (ReachedDest()) {
        if (!sees) {
            destX = lastSeenX;
            destY = lastSeenY;
            return;
        }
        if (detour) {
            if (Detour()) return;
            goto detours;
        }
        goto notDetouring;
    }
    if (destX == lastSeenX && destY == lastSeenY && sees) goto engage;
    if (detour) {
        int r = MoveToDest();
        if (r) {
            if (r == -1) BashDoor();
            return;
        }
        goto detours;
    }
notDetouring:
    if (sees) goto engage;
    moveAngle = angleToPlayer;
    if (stats->aiClass > 9) {
        destX = lastSeenX;
        destY = lastSeenY;
        moveAngle = AngleTo(destX, destY);
        if (!(Distance(x, y, destX, destY) < 1.0f)) {
            int r = MoveToDest();
            if (r == 1) return;
            if (r != -1) goto detours;
            BashDoor();
            return;
        }
    }
    aiMode = spawn->aiMode;
    model = anim.Loop(0);
    return;
engage:
    moveAngle = angleToPlayer;
    if (!(dist > 1.5f)) {
        moveAngle = AngleTo(gPlayer.x, gPlayer.y);
        int a = (moveAngle + 180) % 360;
        destX = SinDeg(a) * radius + gPlayer.x;
        destY = gPlayer.y - CosDeg(a) * radius;
        chaseDist = 5;
        goto resetAnim;
    }
    {
        int cd = chaseDist;
        if ((float)(cd + 2) < dist) {
            destX = SinDeg(angleToPlayer) * (float)chaseDist + gPlayer.x;
            destY = gPlayer.y - CosDeg(moveAngle) * (float)chaseDist;
            if (TILE((int)destX, (int)destY) < 0x14) {
                if (MoveToDest()) return;
                goto detours;
            }
            goto resetAnim;
        }
        if ((float)cd > dist) {
            moveAngle = (rand() % 90 + angleToPlayer + 135) % 360;
            int a = (moveAngle + 180) % 360;
            destX = SinDeg(a) * 6.0f + gPlayer.x;
            destY = gPlayer.y - CosDeg(a) * 6.0f;
            if (TILE((int)destX, (int)destY) < 0x14) {
                if (MoveToDest()) return;
                goto detours;
            }
            timer++;
            if (timer > 5) {
                chaseDist = -1;
                timer = 0;
            }
        }
    }
resetAnim:
    if (anim.curIndex == 0) return;
    model = anim.Loop(0);
    return;
detours: {
    detour = 1;
    detourAngle = angleToPlayer;
    int r0 = rand() % 5;
    for (int i = 0; i < 5; i++) {
        int a = ((gDetourAngles[(r0 + i) % 5] + angleToPlayer + 45) / 90 % 4) * 90;
        moveAngle = a;
        destX = SinDeg(a) * (stats->moveStep * 5.0f) + x;
        destY = y - CosDeg(moveAngle) * (stats->moveStep * 5.0f);
        if (MoveToDest() == 1) return;
    }
    model = anim.Loop(0);
    action = 5;
    timer = (int8_t)(rand() % 5);
    moveAngle = savedAngle;
}
}

// 0x406630
int Monster::ReachedDest() { return Distance(x, y, destX, destY) < stats->radius ? 1 : 0; }

// 0x406670
int Monster::FacingPlayer() {
    int d = angle - angleToPlayer;
    if (d < 45 && d > -45) return 1;
    if (d >= 45 && d > 315) return 1;
    return 0;
}

// 0x4066a0
void Monster::Waiting() {
    timer--;
    if (aiMode != 3) moveAngle = AngleTo(gPlayer.x, gPlayer.y);
    if (timer > 0 && gPlayer.tileX == (int)lastSeenX && gPlayer.tileY == (int)lastSeenY) return;
    action = 1;
    timer = 0;
}

// 0x406720
void Monster::Attacking() {
    timer++;
    if (timer == (int)((uint32_t)anim.numFrames >> 1)) {
        AttackPlayer();
        PlaySound(stats->attackSound, tileX, tileY);
        return;
    }
    if (anim.AtEnd()) {
        action = 5;
        timer = (int8_t)(rand() % (uint8_t)stats->idleDiv + 1);
    }
}

// 0x4067a0: start walking toward moveAngle; -1 = blocked by a door
int Monster::MoveToDest() {
    if (CanMove(moveAngle, stats->moveStep)) {
        action = 2;
        timer = 0;
        if (anim.curIndex != 1) model = anim.Loop(1);
        return 1;
    }
    if (stats->opensDoors) {
        int tx = (int)(SinDeg(moveAngle) + x);
        int ty = (int)(y - CosDeg(moveAngle));
        uint8_t t = TILE(tx, ty);
        if (t == 0x1a || t == 0x1b) return -1;
    }
    return 0;
}

// 0x406850
void Monster::Walking() {
    if (CanMove(angle, stats->moveStep)) {
        x = SinDeg(angle) * stats->moveStep + x;
        y = y - CosDeg(angle) * stats->moveStep;
    } else {
        action = 1;
        timer = 0;
        model = anim.Loop(0);
    }
    timer++;
    if (timer % 5 == 0 && aiMode != 3) Pursue();
}

// 0x4068f0
void Monster::AIFlee() {
    if (!TurnToward()) return;
    switch (action) {
    case 1:
        if (!FleeStep()) {
            action = 5;
            timer = (int8_t)(rand() % 10);
        }
        break;
    case 2:
        Fleeing();
        break;
    case 5:
        Waiting();
        break;
    }
}

// 0x406950
int Monster::FleeStep() {
    int base = angle;
    int found = 0;
    moveAngle = (AngleTo(gPlayer.x, gPlayer.y) + 180) % 360;
    for (int i = 0; i < 4; i++) {
        if (MoveToDest()) found = 1;
        else moveAngle = gFleeAngles[i] + base;
    }
    if (!found) moveAngle = base;
    return found;
}

// 0x4069d0
void Monster::Fleeing() {
    timer++;
    if (timer == 4 && !FleeStep()) goto wait;
    if (CanMove(angle, stats->moveStep)) {
        x = SinDeg(angle) * stats->moveStep + x;
        y = y - CosDeg(angle) * stats->moveStep;
        return;
    }
    if (Distance(x, y, gPlayer.x, gPlayer.y) < 4.0f) {
        action = 1;
        timer = 0;
        return;
    }
wait:
    action = 5;
    timer = (int8_t)(rand() % 10);
}

// 0x406aa0: scripted death (aiMode 8)
void Monster::AIDeathAnim() {
    switch (timer) {
    case 0:
        if (anim.AtEnd()) {
            timer++;
            model = anim.Hold(stats->deathAnim);
        }
        break;
    case 1:
        if (anim.AtEnd()) state = 0x80;
        break;
    }
}

// 0x406b00
void Monster::AlertAnim() {
    if (!anim.AtEnd()) return;
    aiMode = 1;
    action = 1;
    model = anim.Loop(0);
}

// 0x406b30: can the monster see (px,py)?
int Monster::CanSee(float px, float py) {
    if (!gPlayer.noise) {
        if (gLightMap[(int)px * 65 + (int)py] <= 4) {
            if (DistanceB(px, py, x, y) > 4.0) return 0;
        }
    }
    if (gPlayer.invisibility) {
        if (rand() % 100 > stats->aiClass) return 0;
    }
    if (VIS(tileX, tileY) == 0xff) return 0;
    return LineOfSight(x, y, px, py, 0) ? 1 : 0;
}

// 0x406bf0
int Monster::AngleTo(float px, float py) { return AngleBetween(x, y, px, py); }

// 0x406c10
int Monster::InAttackRange(float px, float py, int) {
    if (Distance(x, y, px, py) < stats->reach && FacingPlayer()) return 1;
    return 0;
}

// 0x406c60: can the monster step 'step' in direction 'a'?
int Monster::CanMove(int a, float step) {
    float na = -(float)a;
    float nx = 0, ny = 0;
    for (int i = 0; i < 3; i++) {
        Vec3 v;
        v.x = gMoveProbes[i].x * step;
        v.y = gMoveProbes[i].y;
        v.z = gMoveProbes[i].z * step;
        RotateY(&v, &v, na);
        nx = v.x + x;
        ny = v.z + y;
        uint8_t t = TILE((int)nx, (int)ny);
        if (t >= 0x14 || t == 2) return 0;
        if (!stats->ignoresFloor && t >= 6 && t <= 8) return 0;
    }
    if (CircleOverlap(nx, ny, radius, gPlayer.x, gPlayer.y, 0.5f)) return 0;
    if (MonsterCellOccupied((int)nx, (int)ny) && BlockedByMonster(nx, ny)) return 0;
    return 1;
}

// 0x406db0
int Monster::BlockedByMonster(float nx, float ny) {
    MonsterCell *c = MonsterCellAt((int)nx, (int)ny);
    int cnt = c->count;
    for (int i = 0; cnt > 0 && i < 8; i++) {
        uint8_t id = c->ids[i];
        if (id == 0xff) continue;
        Monster *m = GetMonster(gCurLevel, id);
        if (m->state != 0x80 && m->stats && m->stats->blocks) {
            if (CircleOverlap(nx, ny, radius, m->x, m->y, m->radius)) return 1;
        }
        cnt--;
    }
    return 0;
}

// 0x406e80: the player hits the monster in melee
int Monster::TakeHit() {
    uint8_t canHurt = 1;
    if (state == 0x80) return 0;
    MonsterStats *st = stats;
    if (st->weaponLevel > gWeaponLevels[gEquip.weaponType]) {
        ShowMessage(gMsg[228] /* Your weapon is ineffective */, gColorWhite);
        canHurt = 0;
    }
    if (attitude) {
        SetAttitudeOfType(type, 0);
        if (stats->allyType != -1) SetAttitudeOfType((uint8_t)stats->allyType, 0);
    }
    int toHit = rand() % gStats.dex + (gWeaponDamage[gEquip.weaponType] >> 1) + 1;
    int def = rand() % stats->defence + 1;
    int diff = gEquip.difficulty;
    if (diff == 2) toHit = toHit * 120 / 100;
    if (diff == 3) toHit = toHit * 100 / 120;
    int dmg = rand() % ((int)(float)gStats.str + 1) + gWeaponDamage[gEquip.weaponType] + 1;
    uint8_t hit;
    if (holdTime || fearTime || (def <= toHit && dmg > 0)) hit = canHurt;
    else hit = 0;
    if (hit) {
        if (holdTime) dmg += dmg;
        hp -= dmg;
        if (dmg) BloodSplat(x, y, gPlayer.angle, stats->bloodType);
        if (hp <= 0) {
            Die(1);
        } else {
            if (hp <= stats->fleeHp) {
                aiMode = 2;
                action = 1;
                chaseDist = 6;
                timer = 0;
                fearTime = rand() % 10 + 15;
            }
            PlaySound(stats->hurtSound, tileX, tileY);
        }
        if (gPlayer.invisibility) gPlayer.invisibility = gTicks - 1;
        if (stats->acidic && rand() % 100 < 50) ExpireEnchantments();
        if (chaseDist < 5) chaseDist++;
    }
    PlaySound(gHitSounds[rand() % 3 + gEquip.weaponType * 3], -1, -1);
    MaybeBreakWeapon();
    return 1;
}

// 0x407100: shield bash
int Monster::ShieldHit() {
    if (state == 0x80) return 0;
    int toHit = rand() % 10 + 1;
    int def = rand() % stats->defence + 1;
    int dmg = rand() % 10 + 1;
    if (!(holdTime || fearTime || (def <= toHit && dmg > 0))) return 0;
    hp -= dmg;
    BloodSplat(x, y, rand() % 359, stats->bloodType);
    if (hp > 0) return 0;
    Die(0);
    return 1;
}

// 0x4071b0
int Monster::ProjectileHit(Projectile *p) {
    if (!p->active || state == 0x80) return 0;
    if (!CircleOverlap(x, y, radius, p->x, p->y, 0.3f)) return 0;
    ApplyProjectile(p);
    return 1;
}

// 0x407200
void Monster::ApplyProjectile(Projectile *p) {
    int dmg = p->power;
    uint8_t byPlayer = 0;
    MonsterStats *st = stats;
    switch (p->kind) {
    case 0:
        byPlayer = 1;
        dmg = (100 - st->fireResist) * dmg / 100;
        if (dmg < 0) ShowMessage(gMsg[118] /* The MONSTER appears healed!! */, gColorWhite);
        break;
    case 1:
        byPlayer = 1;
        dmg = (100 - st->coldResist) * dmg / 100;
        if (dmg < 0) ShowMessage(gMsg[118] /* The MONSTER appears healed!! */, gColorWhite);
        else p->power -= dmg;
        break;
    case 2:
        byPlayer = 1;
        /* fall through */
    case 4: {
        int toHit = rand() % gStats.dex + 1;
        int def = rand() % st->defence + 1;
        if (toHit < def) return;
        BloodSplat(x, y, p->angle + 180, st->bloodType);
        break;
    }
    case 3:
        byPlayer = 1;
        dmg = (100 - st->magicResist) * dmg / 100;
        break;
    case 5:
        dmg = (100 - st->fireResist) * dmg / 100;
        break;
    case 6:
        dmg = (100 - st->coldResist) * dmg / 100;
        break;
    default:
        break;
    }
    hp -= dmg;
    if (hp > stats->maxHp) hp = stats->maxHp;
    if (hp <= 0) {
        p->power = abs(hp);
        Die(byPlayer);
    }
    if (byPlayer && attitude) {
        SetAttitudeOfType(type, 0);
        if (stats->allyType != -1) SetAttitudeOfType((uint8_t)stats->allyType, 0);
    }
}

// 0x407420
int Monster::Die(int byPlayer) {
    if (state == 0x80 || action == 4) return 0;
    invisible = 0;
    killedByPlayer = (uint8_t)byPlayer;
    GainExperience(xp);
    xp = 0;
    moveAngle = angle;
    action = 4;
    aiMode = 3;
    model = anim.Hold(stats->deathAnim);
    holdTime = 0;
    fearTime = 0;
    if (spawn->type == 0x26) SpawnDemonShade(x, y, angle);
    if (TILE(tileX, tileY) == 5) ReleasePlate(tileX, tileY);
    if (gPlayer.invisibility) gPlayer.invisibility = gTicks - 1;
    PlaySound(stats->deathSound, (int)x, (int)y);
    return 1;
}

// 0x407530: the death animation has finished
void Monster::Remove() {
    char buf[64];
    UnmarkMonster(tileX, tileY, index);
    if (killedByPlayer == 1) {
        sprintf(buf, "%s%s", gMsg[28] /* You have slain a  */, *stats->name);
        ShowMessage(buf, gColorRed);
    }
    if (!TileHeight(tileX, tileY) && stats->bloodType) BloodPool(tileX, tileY, stats->bloodType);
    int a = angle;
    SpawnDrop *drop = (SpawnDrop *)drops;
    for (int k = 3; k; k--, drop++) {
        if (drop->item == -1) continue;
        int ang = a;
        for (int d = 0; d < 360; d += 45, ang += 45) {
            int tx = (int)(SinDeg(ang) + x);
            int ty = (int)(y - CosDeg(ang));
            if (TILE(tx, ty) == 0) {
                DropItem(tx, ty, drop->item, drop->qty);
                drop->item = -1;
                break;
            }
        }
        a += 45;
        if (drop->item == -1) continue;
        DropItem(tileX, tileY, drop->item, drop->qty);
    }
    state = 0x80;
    deathTimer = 0xff;
}

// 0x4076c0
int Monster::HitTest(int mx, int my) {
    if (state == 0x80) return 0;
    return mx > rect.left && mx < rect.right && my > rect.top && my < rect.bottom;
}

// 0x407700: "death" spell
int Monster::SpellKill() {
    if (state == 0x80) return 0;
    int r = rand() % 50 - (int)((float)gStats.intel * -2.5f) + 1;
    uint8_t res = stats->spellResist;
    if (res && r < res) {
        ShowMessage(gMsg[72] /* Your spell has no effect */, gColorWhite);
        return 0;
    }
    Die(1);
    return 1;
}

// 0x407780
void Monster::SpellDamage(int dmg) {
    if (state == 0x80) return;
    gFlashColor = gColorWhite;
    int r = rand() % 50 - (int)((float)gStats.intel * -2.5f) + 1;
    if (r < stats->magicResist) return;
    if (dmg > hp) dmg /= 2;
    hp -= dmg;
    if (hp <= 0) {
        Die(1);
        BloodSplat(x, y, rand() % 360, stats->bloodType);
        BloodSplat(x, y, rand() % 360, stats->bloodType);
    }
    BloodSplat(x, y, rand() % 360, stats->bloodType);
}

// 0x407870: "deaths door" spell
int Monster::SpellWeaken() {
    char buf[76];
    int r = rand() % 50 - (int)((float)gStats.intel * -2.5f) + 1;
    MonsterStats *st = stats;
    if (r < st->spellResist) {
        ShowMessage(gMsg[72] /* Your spell has no effect */, gColorWhite);
        return 0;
    }
    hp = 6 - gStats.intel / 4;
    if (hp <= 0) hp = 1;
    sprintf(buf, "%s%s", *st->name, gMsg[39] /*  is a Deaths door!! */);
    ShowMessage(buf, gColorBlue);
    return 1;
}

// 0x407930: the monster's attack lands on the player
void Monster::AttackPlayer() {
    MonsterStats *st = stats;
    if (st->specialChance && rand() % 100 < st->specialChance) {
        SpecialAttack();
        return;
    }
    if (st->drainChance && rand() % 100 < stats->drainChance) {
        int r = rand() % 200;
        gStats.xp += -100 - r;
        ShowMessage(gMsg[143] /* Experience drain!! */, gColorAzure);
    }
    switch (stats->flags & 0x7f) {
    case 1: FireBreath(x, y, angle); break;
    case 2: SparkDirected(x, y, 60.0f, angle); break;
    }
    float hx = SinDeg(angle) * stats->reach + x;
    float hy = y - CosDeg(angle) * stats->reach;
    if (!CircleOverlap(gPlayer.x, gPlayer.y, 0.5f, hx, hy, 0.0f)) return;
    int atk = rand() % stats->dmgRange + 1;
    int def = gArmourDefence[gEquip.armourType] + gEquip.shield;
    if (gPlayer.stoneSkin) def += 2;
    int r = rand() % (gStats.tou + def) + 1;
    if (gEquip.difficulty == 2) r = r * 120 / 100;
    if (gEquip.difficulty == 3) r = r * 100 / 120;
    if (gPlayer.magicShield) r = r * 120 / 100;
    if (r > atk) return;
    if (stats->poisonChance && rand() % 100 < stats->poisonChance) {
        if (gPlayer.poisoned < 5) gPlayer.poisoned++;
        ShowMessage(gMsg[46] /* You have been poisoned!! */, gColorGreen);
        UpdatePoisonDisplay(1);
    }
    st = stats;
    int dmg = rand() % (st->dmgExtra + st->dmgBase) + 1;
    int def2 = gArmourDefence[gEquip.armourType] + gEquip.shield;
    if (gPlayer.stoneSkin) def2 += 2;
    dmg -= rand() % (def2 + 1);
    if ((type == 5 || (type > 0x1a && type <= 0x1c)) && gPlayer.resistUndead) {
        dmg -= (int)((float)dmg * ((float)gStats.intel * 0.05f));
    }
    if (dmg <= 0) return;
    DamagePlayer(dmg, 1);
    int behind = 0;
    int d = (AngleBetween(x, y, gPlayer.x, gPlayer.y) + 180) % 360 - gPlayer.angle;
    if (d < 270 && d > 90) behind = 1;
    if ((float)dmg / (float)gStats.maxHp > 0.1f) PlayerKnockback(behind);
    AddWound(stats->b18, behind);
}

// 0x407c90: steal something from the player and run away
void Monster::SpecialAttack() {
    int r = rand() % 5;
    if (r == 0) {
        if (!gPlayer.gold) return;
        unsigned amt = (unsigned)rand() % gPlayer.gold;
        AddDrop(0x79, (int)amt);
        gPlayer.gold -= amt;
    } else if (r == 1) {
        int k = rand() % 26;
        if (!gRuneCounts[k]) return;
        AddDrop(k, 0);
        gRuneCounts[k]--;
    } else {
        int k = rand() % 30;
        if (gInventory[k].item == -1) return;
        AddDrop(gInventory[k].item, gInventory[k].qty);
        RemoveInventoryItem(k);
    }
    ShowMessage(gMsg[62] /* You have been pilfered!! */, gColorAzure);
    aiMode = 2;
    action = 1;
    timer = 0;
    fearTime = 100000;
}

// 0x407d80
int Monster::IsVisible() {
    if (state == 0x80) return 0;
    if (!LineOfSight(x, y, gPlayer.x, gPlayer.y, 0)) return 0;
    if (VIS(tileX, tileY) == 0xff) return 0;
    return 1;
}

// 0x407dd0
void Monster::FinishDying() {
    if (!anim.AtEnd()) return;
    Remove();
    action = 1;
    timer = 0;
}

// 0x407e00: leave remains on the floor
void Monster::LeaveCorpse() {
    if (!deathTimer) return;
    int r = rand() % 4 + 0x8f;
    switch (stats->corpse) {
    case 1: DropItem(tileX, tileY, r, 0); break;
    case 2: DropItem(tileX, tileY, 0x6e, 0); break;
    case 3:
        DropItem(tileX, tileY, 0x6e, 0);
        DropItem(tileX, tileY, r, 0);
        break;
    case 4: DropItem(tileX, tileY, 0x8a, 0); break;
    case 5: DropItem(tileX, tileY, 0x8b, 0); break;
    case 6: DropItem(tileX, tileY, 0x8c, 0); break;
    }
    deathTimer = 0;
}

// 0x407f00
int Monster::IsActive() { return action != 4 && state != 0x80; }

// 0x407f20
void Monster::AddDrop(int item, int qty) {
    SpawnDrop *d = (SpawnDrop *)drops;
    for (int i = 0; i < 3; i++) {
        if (d[i].item == -1) {
            d[i].item = (int16_t)item;
            d[i].qty = (int16_t)qty;
            return;
        }
    }
}

// 0x407f60
void Monster::CheckTileTrigger() {
    if (state == 0x80) return;
    if (TILE(tileX, tileY) == 9) PressurePlate(tileX, tileY, 0);
}

// 0x407fa0
void Monster::RegenHp() {
    char buf[52];
    MonsterStats *st = stats;
    uint8_t rate = st->regenRate;
    if (!rate) return;
    unsigned period = (unsigned)(300 / rate);
    if (gTicks % period) return;
    if (hp >= st->maxHp) return;
    hp++;
    if (VIS(tileX, tileY) >= 0xfe) return;
    sprintf(buf, "%s%s", *st->name, gMsg[231] /*  regenerating!! */);
    ShowMessage(buf, gColorWhite);
}

// 0x408040
int Monster::IsHostileTarget() {
    if (!IsActive()) return 0;
    if (VIS(tileX, tileY) >= 0xfe) return 0;
    if (stats->b28) return 0;
    if (attitude) return 0;
    return 1;
}

// 0x408080: dead monster rises again
void Monster::Regenerate() {
    float d = Distance(gPlayer.x, gPlayer.y, x, y);
    if (deathTimer <= 2) return;
    if (MonsterCellOccupied(tileX, tileY)) return;
    if (!(d > 1.5f)) return;
    if (rand() % 20 <= 13) return;
    state = 0;
    aiMode = 1;
    action = 6;
    model = anim.Reverse(stats->deathAnim, &model, 0);
    deathTimer = 0;
    hp = stats->maxHp >> 1;
}

// ---------------------------------------------------------------------------
// Monster occupancy map: 9 bytes per tile (count + up to 8 monster ids)

static MonsterCell gMonsterMapDummy;

MonsterCell *MonsterCellAt(int x, int y) {
    int i = x * 65 + y;
    if (i < 0 || i >= 70 * 65) return &gMonsterMapDummy;
    return &gMonsterMap[i];
}

// 0x408130
void ClearMonsterMap() {
    memset(gMonsterMap, 0xff, sizeof(gMonsterMap));
    for (int i = 0; i < 70 * 65; i++) gMonsterMap[i].count = 0;
}

// 0x408160
int MarkMonster(int x, int y, int id) {
    MonsterCell *c = MonsterCellAt(x, y);
    if (c->count >= 8) return 0;
    for (int i = 0; i < 8; i++) {
        if (c->ids[i] == 0xff) {
            c->ids[i] = (uint8_t)id;
            break;
        }
    }
    c->count++;
    return 1;
}

// 0x4081c0: remove a monster from the 3x3 block around (x,y)
void UnmarkMonster(int x, int y, int id) {
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            MonsterCell *c = MonsterCellAt(x + dx, y + dy);
            for (int i = 0; i < 8; i++) {
                if (c->ids[i] == (uint8_t)id) {
                    c->ids[i] = 0xff;
                    c->count--;
                }
            }
        }
    }
}

// 0x408220
int MonsterCellOccupied(int x, int y) { return MonsterCellAt(x, y)->count != 0; }

// 0x408250: monster whose body overlaps a circle at (x,y), 0xff if none
uint8_t MonsterAtPoint(float x, float y, float r) {
    MonsterCell *c = MonsterCellAt((int)x, (int)y);
    for (int i = 0; i < 8; i++) {
        uint8_t id = c->ids[i];
        if (id == 0xff) continue;
        Monster *m = GetMonster(gCurLevel, id);
        if (CircleOverlap(x, y, r, m->x, m->y, m->radius)) return c->ids[i];
    }
    return 0xff;
}
