// Path finding, projectiles, dart traps, rats, summoned servants and the
// dirty-rectangle save/restore list (0x408310-0x40aa20).
#include "game.h"

#define TILE(x, y) gLevelMap[gMapRow[(y)] + (x)]
#define VIS(x, y) gMapVis[(x) * 65 + (y)]
#define LIGHT(x, y) gLightMap[(x) * 65 + (y)]

// ---------------------------------------------------------------------------
// Path planner used for click-to-walk (0x408310-0x408d60).
// ---------------------------------------------------------------------------

struct Waypoint {
    float x, y;
    float dist;            // distance from the previous waypoint
};

static int gPathAngle;             // 0x4af290
static float gPathX, gPathY;       // 0x4af294 current probe position
static float gPathTY, gPathTX;     // 0x4af29c target (note: y stored first)
static Waypoint gPath[21];         // 0x4af2a8 (20 used, +1 read by PathSmooth)
static int gPathCount;             // 0x4af48c
static int gPathCur;               // 0x4af490
static unsigned gPathDir;          // 0x4af494

static const Vec3 kPathProbes[3] = {    // 0x4482e8
    {-0.35f, 0.0f, -2.0f}, {0.35f, 0.0f, -2.0f}, {0.0f, 0.0f, -2.0f},
};
static const int kAroundAngles[6] = {0, 90, 270, 0, 270, 90}; // 0x448310

static int PathNear(float x0, float y0, float x1, float y1, float r);
static int PathAdd(float x, float y);
static int PathStep(float *px, float *py, int angle, int steps);
static int PathFindOpening(float *px, float *py, int *pangle);
static int PathWalk(float x0, float y0, float *px, float *py);
static int PathAround(float *px, float *py, int *pangle, int side);
static void PathSmooth();

// 0x408310: plan a route from (x,y) to (tx,ty). Returns 1 if a path exists.
int PlanPath(float x, float y, int angle, float tx, float ty)
{
    (void)angle;
    gPathCount = 0;
    gPathCur = 0;
    gPathTX = tx;
    gPathTY = ty;
    gPathX = x;
    gPathY = y;
    gPathAngle = AngleBetween(x, y, tx, ty);
    if (Distance(x, y, tx, ty) < 0.5f) return 0;
    PathAdd(gPathX, gPathY);
    if (!PathNear(gPathX, gPathY, gPathTX, gPathTY, 0.12f)) {
        do {
            gPathAngle = AngleBetween(gPathX, gPathY, gPathTX, gPathTY);
            float nx = gPathX, ny = gPathY;
            if (PathStep(&nx, &ny, gPathAngle, 1)) {
                gPathX = nx;
                gPathY = ny;
            } else {
                gPathX = nx;
                gPathY = ny;
                if (!PathFindOpening(&gPathX, &gPathY, &gPathAngle)) return 0;
                if (!PathAdd(gPathX, gPathY)) return 0;
                int saved = gPathCount;
                if (!PathAround(&gPathX, &gPathY, &gPathAngle, 0)) {
                    gPathCount = saved;
                    if (!PathAround(&gPathX, &gPathY, &gPathAngle, 1)) return 0;
                }
            }
        } while (!PathNear(gPathX, gPathY, gPathTX, gPathTY, 0.12f));
    }
    if (!PathAdd(gPathX, gPathY)) return 0;
    PathSmooth();
    if (gPathCount < 2) return 0;
    for (int i = 0; i < gPathCount; i++) {
        if (i == 0)
            gPath[i].dist = 0;
        else
            gPath[i].dist = Distance(gPath[i].x, gPath[i].y, gPath[i - 1].x, gPath[i - 1].y);
    }
    return 1;
}

// 0x408590
static int PathNear(float x0, float y0, float x1, float y1, float r)
{
    return Distance(x0, y0, x1, y1) < r * 1.5f;
}

// 0x4085d0
static int PathAdd(float x, float y)
{
    if (gPathCount >= 20) return 0;
    gPath[gPathCount].x = x;
    gPath[gPathCount].y = y;
    gPathCount++;
    return 1;
}

// 0x408610: move up to 'steps' steps of 0.12 along angle, checking three
// probe points ahead. On a blocked step the last free position is returned.
static int PathStep(float *px, float *py, int angle, int steps)
{
    float x = *px, y = *py;
    if (steps > 0) {
        float negAngle = (float)-angle;
        for (int i = 0; i < steps; i++) {
            float prevX = x, prevY = y;
            x = SinDeg(angle) * 0.12f + x;
            y = y - CosDeg(angle) * 0.12f;
            for (int p = 0; p < 3; p++) {
                Vec3 v;
                v.x = kPathProbes[p].x * 0.12f;
                v.y = kPathProbes[p].y;
                v.z = kPathProbes[p].z * 0.12f;
                RotateY(&v, &v, negAngle);
                int ty = (int)(v.z + y);
                int tx = (int)(v.x + x);
                if (TILE(tx, ty) >= 0x14) {
                    *px = prevX;
                    *py = prevY;
                    return 0;
                }
            }
        }
    }
    *px = x;
    *py = y;
    return 1;
}

// 0x408770: after hitting a wall, check the tile centre can be walked to from
// the last waypoint. (The retry loop in the original can never repeat.)
static int PathFindOpening(float *px, float *py, int *pangle)
{
    float lx = gPath[gPathCount - 1].x;
    float ly = gPath[gPathCount - 1].y;
    float fx = (float)(int)*px + 0.5f;
    float fy = (float)(int)*py + 0.5f;
    if (!PathWalk(lx, ly, &fx, &fy)) return 0;
    *px = fx;
    *py = fy;
    *pangle = AngleBetween(lx, ly, fx, fy);
    return 1;
}

// 0x4088a0: walk in a straight line from (x0,y0) to (*px,*py).
static int PathWalk(float x0, float y0, float *px, float *py)
{
    float cx = x0, cy = y0;
    int ang = AngleBetween(x0, y0, *px, *py);
    if (PathNear(cx, cy, *px, *py, 0.12f)) return 1;
    for (;;) {
        if (!PathStep(&cx, &cy, ang, 1)) {
            *px = cx;
            *py = cy;
            return 0;
        }
        if (PathNear(cx, cy, *px, *py, 0.12f)) return 1;
    }
}

// 0x408960: follow the wall on one side until the target is reachable.
static int PathAround(float *px, float *py, int *pangle, int side)
{
    float cx = *px, cy = *py;
    int sideOff = side * 3;
    gPathDir = 1;
    do {
        float sx = cx, sy = cy;
        float wx = cx, wy = cy;
        int a = AngleBetween(wx, wy, gPathTX, gPathTY);
        int snapped = ((kAroundAngles[gPathDir + sideOff] + a + 45) / 90 % 4) * 90;
        int diff = a - snapped;
        int wallAng;
        if ((diff > 0 && diff < 180) || diff < -180)
            wallAng = (snapped + 90) % 360;
        else
            wallAng = (snapped + 270) % 360;
        int stuck = 0;
        int restart = 0;
        for (;;) {
            int toTarget = AngleBetween(wx, wy, gPathTX, gPathTY);
            float tx = cx, ty = cy;
            if (PathStep(&tx, &ty, toTarget, 8)) {
                if (!PathAdd(wx, wy)) return 0;
                *px = tx;
                *py = ty;
                *pangle = snapped;
                return 1;
            }
            tx = cx;
            ty = cy;
            if (PathStep(&tx, &ty, wallAng, 8)) {
                if (!PathAdd(wx, wy)) return 0;
                cx = tx;
                cy = ty;
                restart = 1;
                break;
            }
            tx = cx;
            ty = cy;
            if (PathStep(&tx, &ty, snapped, 1)) {
                cx = tx;
                cy = ty;
            } else {
                cx = sx;
                cy = sy;
                stuck = 1;
            }
            if (stuck) break;
            wx = cx;
            wy = cy;
        }
        if (restart)
            gPathDir = 1;
        else
            gPathDir++;
    } while (gPathDir < 3);
    return 0;
}

// 0x408b80: drop waypoints that can be skipped.
static void PathSmooth()
{
    if (gPathCount < 4) return;
    int i = 0;
    while (i < gPathCount - 2) {
        float ax = gPath[i + 2].x, ay = gPath[i + 2].y;
        if (PathWalk(gPath[i].x, gPath[i].y, &ax, &ay)) {
            for (int j = i + 1; j < gPathCount; j++) gPath[j] = gPath[j + 1];
            gPathCount--;
            i = 0;
        } else {
            i++;
        }
    }
}

// 0x408c50: advance the player along the planned path. Returns 0 at the end.
int FollowPath(Player *pl)
{
    int advanced = 0;
    float segDist = 0;
    Waypoint *w = &gPath[gPathCur];
    if (gPathCur > 0)
        segDist = Distance(gPath[gPathCur - 1].x, gPath[gPathCur - 1].y, pl->x, pl->y);
    for (;;) {
        if (!PathNear(pl->x, pl->y, w->x, w->y, fabsf(pl->speed)) && segDist <= w->dist)
            break;
        advanced++;
        gPathCur++;
        if (gPathCur >= gPathCount) return 0;
        w = &gPath[gPathCur];
        segDist = 0;
    }
    if (advanced) {
        pl->destX = w->x;
        pl->destY = w->y;
        if (gPathCur > 0) {
            pl->x = gPath[gPathCur - 1].x;
            pl->y = gPath[gPathCur - 1].y;
        }
        int a = AngleBetween(pl->x, pl->y, w->x, w->y);
        pl->angle = a;
        pl->turnAngle = a;
    }
    return 1;
}

// ---------------------------------------------------------------------------
// Projectiles (0x408d60-0x4095e0).
// ---------------------------------------------------------------------------

// 0x408d60
void LoadProjectileModels()
{
    if (!gBoltModel.Load("gamedat\\bolt.omt")) FatalError("cannot load bolt model");
    if (!gFireballModel.Load("gamedat\\fireball.omt")) FatalError("cannot load fireball model");
    if (!gLightningModel.Load("gamedat\\lightnin.omt")) FatalError("cannot load lightning model");
    if (!gMissileModel.Load("gamedat\\magMiss.omt")) FatalError("cannot load missile model");
    if (!gBoltTex.Load("gamedat\\bolt.tex")) FatalError("cannot load bolt texture");
    if (!gFireballTex.Load("gamedat\\fireball.tex")) FatalError("cannot load fireball texture");
    if (!gLightningTex.Load("gamedat\\lightnin.tex")) FatalError("cannot load lightning texture");
    if (!gMissileTex.Load("gamedat\\magMiss.tex")) FatalError("cannot load magic missile texture");
    gBoltModel.SetTexture(&gBoltTex);
    gFireballModel.SetTexture(&gFireballTex);
    gLightningModel.SetTexture(&gLightningTex);
    gMissileModel.SetTexture(&gMissileTex);
}

// 0x408ea0
void FreeProjectileModels()
{
    gBoltTex.Free();
    gBoltModel.Free();
    gFireballModel.Free();
    gFireballTex.Free();
    gLightningModel.Free();
    gLightningTex.Free();
    gMissileModel.Free();
    gMissileTex.Free();
}

// 0x408ef0
int FireProjectile(float x, float y, int angle, int kind, int flags)
{
    int i = 0;
    while (gProjectiles[i].active == 1) {
        if (++i == 10) return 0;
    }
    int ty = (int)y;
    int tx = (int)x;
    if (!TileIsOpen(tx, ty)) return 0;
    Projectile *p = &gProjectiles[i];
    p->x = x;
    p->y = y;
    p->angle = angle;
    p->kind = kind;
    p->light = 0x1f;
    p->flags = flags;
    p->power = ProjectileDamage(kind);
    p->scale = 0;
    p->active = 1;
    PlaySound(gProjFireSound[kind], tx, ty);
    return 1;
}

// 0x408fd0
void UpdateProjectiles()
{
    for (int i = 0; i < 10; i++) {
        Projectile *p = &gProjectiles[i];
        switch (p->active) {
        case 0: break;
        case 1: ProjectileFly(p, i); break;
        case 2: ProjectileExploding(p, i); break;
        case 13:
            g_5c5824 = 1;
            p->active = 0;
            break;
        }
    }
}

// 0x409030
void ProjectileFly(Projectile *p, int index)
{
    (void)index;
    if (ProjectileHitsMonster(p) || ProjectileHitsPlayer(p)) {
        ProjectileExplode(p);
        return;
    }
    float nx = SinDeg(p->angle) * gProjSpeed[p->kind] + p->x;
    float ny = p->y - CosDeg(p->angle) * gProjSpeed[p->kind];
    int tx = (int)nx;
    int ty = (int)ny;
    if (!TileIsOpen(tx, ty)) {
        ProjectileExplode(p);
        return;
    }
    p->x = nx;
    p->y = ny;
    p->tileX = tx;
    p->tileY = ty;
    if (p->kind == 0 || p->kind == 5) FireTrail(nx, ny);
}

// 0x409100
int ProjectileHitsPlayer(Projectile *p)
{
    if (!CircleOverlap(gPlayer.x, gPlayer.y, 0.5f, p->x, p->y, 0.3f)) return 0;
    if (p->kind != 7) {
        DamagePlayer(p->power, 1);
        return 1;
    }
    // Spit: disease instead of damage.
    gPlayer.diseaseTime = 540;
    gPlayer.diseased++;
    UpdatePoisonDisplay(1);
    ShowMessage(gMsg[112] /* You have been diseased */, gColorGreen);
    return 1;
}

// 0x409190
void ProjectileExploding(Projectile *p, int index)
{
    (void)index;
    if (--p->timer == 0) p->active = 13;
    p->scale += 0.3f;
    g_5c5824 = 1;
}

// 0x4091c0
int ProjectileExplode(Projectile *p)
{
    p->timer = gProjExplodeTime[p->kind];
    switch (p->kind) {
    case 0:
    case 5:
        FireBreath(p->x, p->y, p->angle + 180);
        // fall through
    case 1:
    case 3:
    case 6:
        p->active = 2;
        PlaySound(gProjExplodeSound[p->kind], p->tileX, p->tileY);
        p->scale = 1.0f;
        break;
    case 2:
        DropArrow(p->tileX, p->tileY);
        // fall through
    case 4:
        p->active = 0;
        g_5c5824 = 1;
        p->active = 0;
        break;
    default:
        p->active = 0;
        break;
    }
    if (p->flags & 1) SummonServant(p->tileX, p->tileY);
    return p->active;
}

// 0x409280
int TileIsOpen(int x, int y)
{
    return TILE(x, y) < 0x14;
}

// 0x4092a0: a spent crossbow bolt lands as an item near where it hit.
void DropArrow(int x, int y)
{
    int item = 0x7c;
    if (rand() % 4 == 1) item = 0x7d;
    if (DropItem(x, y, item, 1) != -1) return;
    float fx = (float)x + 0.5f;
    float fy = (float)y + 0.5f;
    for (int a = 0; a < 360; a += 45) {
        float sx = SinDeg(a) + fx;
        float sy = CosDeg(a) + fy;
        if (LineOfSight(sx, sy, gPlayer.x, gPlayer.y, 0)) {
            if (DropItem((int)sx, (int)sy, item, 1) != -1) return;
        }
    }
}

// 0x409390
void DrawProjectiles()
{
    for (int i = 0; i < 10; i++) {
        Projectile *p = &gProjectiles[i];
        if (!p->active) continue;
        switch (p->kind) {
        case 1:
        case 6:
            SparkBurst(p->x, p->y);
            break;
        case 0:
        case 3:
        case 5:
            g_5c5824 = 1;
            DrawProjectileModel(p, p->active == 2 ? 0x1f : 0x18);
            break;
        case 7:
            DrawSpit(p->x, p->y, p->angle);
            break;
        default:
            DrawProjectileModel(p, LIGHT(p->tileX, p->tileY), true);
            break;
        }
    }
}

// 0x409440
void DrawProjectileModel(Projectile *p, int light, bool tileLit)
{
    FGObject o;
    memset(&o, 0, sizeof(o));
    float x = p->x, y = p->y;
    if (VIS(p->tileX, p->tileY) >= 0xfe) return;
    WorldToScreen(x, y, &p->sx, &p->sy);
    p->sx += 0x1f;
    p->sy -= 20;
    o.x0 = p->sx;
    o.y0 = p->sy;
    o.depth = ScreenDepth(x, y);
    o.model = gProjModels[p->kind];
    o.frame = 0;
    o.rect = nullptr;
    o.angle = (float)p->angle + 45.0f;
    o.type = 0x210;
    o.scale = p->scale;
    o.light = light & 0xffff;
    if (gColorLight && tileLit) o.lrgb = gLightRGB[p->tileX * 65 + p->tileY];
    AddFGObject(&o);
}

// 0x409520
int ProjectileDamage(int kind)
{
    switch (kind) {
    case 0: return Dice(8, 1, 3, gStats.intel);
    case 1: return Dice(5, 2, 4, gStats.intel);
    case 3: return Dice(3, 1, 2, gStats.intel);
    case 4: return Dice(0, 3, 8, 1);
    case 5: return Dice(5, 5, 10, 1);
    case 6: return Dice(10, 5, 10, 1);
    case 2:
    default: return Dice(0, 5, 10, 1);
    }
}

// 0x4095e0: the player stepped on (x,y); fire any dart traps linked to it.
void PressurePlate(int x, int y, int showMsg)
{
    // (the table has no -1 end marker: the original ran on past its end)
    for (Trap *t = gTraps; t < gTraps + 125 && t->level != -1; t++) {
        if (gCurLevel + 1 != t->level) continue;
        if (x != t->trigX || y != t->trigY) continue;
        if (gTime <= t->lastTime + 150) continue;
        if (FireProjectile((float)t->srcX + 0.5f, (float)t->srcY + 0.5f, t->angle, t->kind, 0) != 1)
            continue;
        t->lastTime = gTime;
        PlaySound(5, x, y);
        if (showMsg) ShowMessage(gMsg[52] /* You have triggered a trap !! */, gColorWhite);
    }
}

// ---------------------------------------------------------------------------
// Rats (0x4096e0-0x409c50).
// ---------------------------------------------------------------------------

// 0x4096f0 (static initialiser)
void StaticInit_RatModel() { gRatModel.Construct(); }
// 0x409710 (static initialiser)
void StaticInit_RatTex() { gRatTex.Construct(); }

// 0x409720
void LoadRatModel()
{
    if (!gRatModel.Load("gamedat\\rat.omt")) FatalError("Cannot load RAT omt");
    if (!gRatTex.Load("gamedat\\rat.tex")) FatalError("Cannot load RAT tex");
    gRatModel.SetTexture(&gRatTex);
}

// 0x409770
void FreeRatModel()
{
    gRatTex.Free();
    gRatModel.Free();
}

// 0x409790: scatter 'count' rats around randomly chosen points (x,y pairs).
void SpawnRats(int count, int numPoints, int *points)
{
    gNumRats = count;
    for (int i = 0; i < gNumRats; i++)
        RatInit(&gRats[i], &points[(rand() % numPoints) * 2]);
}

// 0x4097e0
void UpdateRats()
{
    for (int i = 0; i < gNumRats; i++) {
        RatUpdate(&gRats[i]);
        RatDraw(&gRats[i]);
    }
}

// 0x409810
void DrawRats()
{
    for (int i = 0; i < gNumRats; i++) RatDraw(&gRats[i]);
}

// 0x409840
void RatInit(Rat *r, int *pt)
{
    int tile;
    do {
        r->state = 0;
        r->x = (float)(rand() % 5 + pt[0] - 2) + 0.5f;
        r->y = (float)(rand() % 5 + pt[1] - 2) + 0.5f;
        r->tileX = (int)r->x;
        r->tileY = (int)r->y;
        tile = TILE(r->tileX, r->tileY);
        r->light = 2;
        r->timer = rand() % 30;
        r->angle = rand() % 360;
    } while (tile != 0);
    r->f18 = 0;
}

// 0x409900
void RatUpdate(Rat *r)
{
    if (gTimeStop) return;
    if (r->state == 0) {
        if (TileLight(r->tileX, r->tileY) > r->light ||
            --r->timer == 0 ||
            DistanceB(gPlayer.x, gPlayer.y, r->x, r->y) < 2.0f) {
            r->state = 2;
            r->timer = 2;
        }
        return;
    }
    if (r->state != 2) return;

    if (TileLight(r->tileX, r->tileY) < 6 && r->timer == 0) {
        r->state = 0;
        r->angle = rand() % 360;
        r->timer = 0x20 - TileLight(r->tileX, r->tileY);
    } else {
        if (DistanceB(gPlayer.x, gPlayer.y, r->x, r->y) < 2.0f)
            r->angle = AngleBetween(gPlayer.x, gPlayer.y, r->x, r->y);
        int tries = 8;
        for (;;) {
            float nx = SinDeg(r->angle) * 0.3f + r->x;
            float ny = r->y - CosDeg(r->angle) * 0.3f;
            int ty = (int)ny;
            int tx = (int)nx;
            if (TILE(tx, ty) < 0x14 && nx > 1.0f && ny > 1.0f &&
                (float)(gMapWidth - 1) > nx && (float)(gMapHeight - 1) > ny) {
                r->x = nx;
                r->y = ny;
                break;
            }
            r->angle = (rand() % 90 + r->angle - 45) % 360;
            if (r->angle < 0) r->angle += 360;
            if (--tries == 0) {
                r->state = 0;
                r->timer = 2;
                r->angle += 90;
                return;
            }
        }
    }
    if (r->timer) r->timer--;
    r->tileX = (int)r->x;
    r->tileY = (int)r->y;
    r->light = TileLight(r->tileX, r->tileY);
}

// 0x409b60
void RatDraw(Rat *r)
{
    FGObject o;
    memset(&o, 0, sizeof(o));
    if (VIS(r->tileX, r->tileY) >= 0xfe) return;
    WorldToScreen(r->x, r->y, &r->sx, &r->sy);
    r->sy += 0x10;
    r->sx += 0x1f;
    o.fx = r->x;
    o.fy = r->y;
    o.x0 = r->sx;
    o.y0 = r->sy;
    o.angle = (float)(r->angle - 135);
    o.type = 0x11;
    o.model = &gRatModel;
    o.frame = 0;
    o.light = LIGHT(r->tileX, r->tileY);
    if (gColorLight) o.lrgb = SmoothLightModel() ? LightRGBAt(r->x, r->y) : gLightRGB[r->tileX * 65 + r->tileY];
    o.depth = ScreenDepth(r->x, r->y);
    AddFGObject(&o);
}

// ---------------------------------------------------------------------------
// Summoned servants (0x409c60-0x40a930).
// ---------------------------------------------------------------------------

// 0x409c60 (static initialiser)
void StaticInit_ServantAnim() { gServantAnim.Construct(); }
// 0x409c80 (static initialiser)
void StaticInit_ServantTex() { gServantTex.Construct(); }

// 0x409c90
void LoadServantModels()
{
    if (!gServantAnim.Load("gamedat\\servant.amt")) FatalError("cannot load servant AMT");
    if (!gServantTex.Load("gamedat\\servant.tex")) FatalError("cannot load servant TEX");
    gServantAnim.SetTexture(&gServantTex);
    if (!gSummonModel.Load("gamedat\\summon.omt")) FatalError("cannot load summon OMT");
    if (!gSummonTex.Load("gamedat\\summon.tex")) FatalError("cannot load summon TEX");
    gSummonModel.SetTexture(&gSummonTex);
    if (!gHordeModel.Load("gamedat\\horde.omt")) FatalError("cannot load horde OMT");
    if (!gHordeTex.Load("gamedat\\horde.tex")) FatalError("cannot load horde TEX");
    gHordeModel.SetTexture(&gHordeTex);
    for (int i = 0; i < 10; i++) gServantAnim.CopyTo(&gServants[i].anim);
}

// 0x409da0: re-attach the shared servant models (after loading a game).
void RelinkServants()
{
    for (int i = 0; i < 10; i++) gServants[i].anim.Share(&gServantAnim, &gServants[i].model);
}

// 0x409dd0
void FreeServantModels()
{
    gServantTex.Free();
    gServantAnim.Free();
    gSummonTex.Free();
    gSummonModel.Free();
    gHordeTex.Free();
    gHordeModel.Free();
}

// 0x409e10
void ClearServants()
{
    for (int i = 0; i < 10; i++) gServants[i].active = 0;
}

// 0x409e30
int SpawnServant(float x, float y)
{
    int i = 0;
    while (gServants[i].active == 1) {
        if (++i == 10) return 0;
    }
    Servant *s = &gServants[i];
    s->x = x;
    s->y = y;
    s->action = 4;
    s->counter = 0;
    s->targetIdx = -1;
    s->angle = rand() % 360;
    s->mode = 1;
    s->wanderAngle = rand() % 360;
    s->life = rand() % 500 + 600;
    s->model = s->anim.Reverse(6, &s->model, 1);
    s->active = 1;
    PlaySound(0x32, (int)x, (int)y);
    return 1;
}

// 0x409f10
void UpdateServants()
{
    for (int i = 0; i < 10; i++)
        if (gServants[i].active) ServantUpdate(&gServants[i]);
}

// 0x409f40
void DrawServants()
{
    for (int i = 0; i < 10; i++)
        if (gServants[i].active) ServantDraw(&gServants[i]);
}

// 0x409f70
void ServantUpdate(Servant *s)
{
    switch (s->mode) {
    case 1: ServantSeek(s); break;
    case 2: ServantFight(s); break;
    case 3: ServantVanish(s); return;
    }
    if (rand() % 150 == 0x45) PlaySound(0x32, (int)s->x, (int)s->y);
    if (--s->life <= 0) {
        s->model = s->anim.Loop(6);
        s->mode = 3;
        s->counter = 0;
    }
}

// 0x40a000: look for a monster to fight, otherwise wander near the player.
void ServantSeek(Servant *s)
{
    if (s->action != 4) {
        ServantWalk(s);
        return;
    }
    int t = ServantFindTarget();
    if (t == -1) {
        if (DistanceB(gPlayer.x, gPlayer.y, s->x, s->y) > 9.0f ||
            (s->x == s->destX && s->y == s->destY)) {
            int rx, ry;
            do {
                rx = rand() % 7 + gPlayer.tileX - 3;
                ry = rand() % 7 + gPlayer.tileY - 3;
            } while ((float)rx == s->destX && (float)ry == s->destY);
            s->destX = (float)rx;
            s->destY = (float)ry;
            s->angle = AngleBetween(s->x, s->y, (float)rx, (float)ry);
        }
    } else {
        s->target = GetMonster(gCurLevel, t);
        ServantSetDest(s, s->target->x, s->target->y);
        s->mode = 2;
        s->targetIdx = t;
        s->angle = AngleBetween(s->x, s->y, s->destX, s->destY);
    }
    s->action = 5;
    s->counter = 0;
}

// 0x40a160
void ServantFight(Servant *s)
{
    if (s->action == 4) {
        if (s->target->state == 0x80) {
            s->targetIdx = -1;
            s->mode = 1;
            s->counter = 0;
            return;
        }
        if (ServantInReach(s)) {
            s->angle = ServantAngleTo(s, s->target->x, s->target->y);
            s->action = 6;
            s->model = s->anim.Loop(rand() % 2 + 2);
            s->counter = 0;
        } else {
            ServantSetDest(s, s->target->x, s->target->y);
            s->angle = ServantAngleTo(s, s->destX, s->destY);
            s->action = 5;
            s->counter = 0;
        }
    } else if (s->action == 6) {
        ServantAttack(s);
    } else if (ServantInReach(s)) {
        s->action = 6;
        s->model = s->anim.Loop(rand() % 2 + 2);
        s->counter = 0;
        s->angle = ServantAngleTo(s, s->target->x, s->target->y);
    } else {
        ServantWalk(s);
    }
}

// 0x40a280
void ServantWalk(Servant *s)
{
    if (s->action != 5) return;
    s->x = s->x - SinDeg(s->angle) * -0.25f;
    s->y = s->y - CosDeg(s->angle) * 0.25f;
    if (++s->counter == 4) {
        s->action = 4;
        s->counter = 0;
    }
}

// 0x40a2e0
void ServantAttack(Servant *s)
{
    if (++s->counter == 1) {
        PlaySound(0x18, (int)s->x, (int)s->y);
        return;
    }
    if (s->anim.AtEnd()) {
        s->counter = 0;
        s->action = 4;
        s->model = s->anim.Loop(1);
        ServantHitTarget(s);
    }
}

// 0x40a350
int ServantAngleTo(Servant *s, float x, float y)
{
    return AngleBetween(s->x, s->y, x, y);
}

// 0x40a370: nearest hostile monster not already taken by too many servants.
int ServantFindTarget()
{
    float dist[50];
    int idx[50];
    int n = 0;
    for (int i = 0; i < 50; i++) {
        Monster *m = GetMonster(gCurLevel, i);
        if (m->IsHostileTarget()) {
            dist[n] = Distance(gPlayer.x, gPlayer.y, m->x, m->y);
            idx[n] = i;
            n++;
        }
    }
    if (n == 0) return -1;
    if (n > 1) {
        for (int i = 0; i < n - 1; i++) {
            for (int j = i; j < n; j++) {
                if (!(dist[i] <= dist[j])) {
                    float fd = dist[i]; dist[i] = dist[j]; dist[j] = fd;
                    int t = idx[i]; idx[i] = idx[j]; idx[j] = t;
                }
            }
        }
    }
    for (int limit = 1; limit != 5; limit++) {
        for (int i = 0; i < n; i++)
            if (!ServantsTargeting(idx[i], limit)) return idx[i];
    }
    return -1;
}

// 0x40a490: are at least 'limit' servants already after monster 'idx'?
int ServantsTargeting(int idx, int limit)
{
    int n = 0;
    for (int i = 0; i < 10; i++)
        if (gServants[i].active == 1 && gServants[i].targetIdx == idx) n++;
    return n >= limit;
}

// 0x40a4d0
int ServantInReach(Servant *s)
{
    Monster *m = GetMonster(gCurLevel, s->targetIdx);
    return Distance(s->x, s->y, m->x, m->y) < 0.8f;
}

// Raw words following the 8-direction offset tables at 0x44b650/0x44b670.
// The original indexes them with an angle (0..359) when it gives up looking
// for a free spot, so the real image contents are reproduced here.
static const int32_t kRaw44b650[368] = {
    0, 1, 1, 1, 0, -1, -1, -1, -1, -1, 0, 1, 1, 1, 0, -1,
    8, 16, 8, 0, -8, -16, -8, 0, -4, 0, 4, 8, 4, 0, -4, -8,
    4, 5, 6, 7, 0, 1, 2, 3, 66, 6, 83, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 66, 66, 6, 6, 81, 66, 6, 6, 6, 66, 6, 6,
    6, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    22, 22, 22, 22, 27, 27, 30, 32, 34, 37, 40, 34, 40, 22, 37, 32,
    0, 0, 0, 0, 43, 43, 43, 46, 47, 47, 47, 47, 51, 51, 51, 51,
    55, 56, 55, 58, 58, 60, 60, 84, 97, 98, 99, 100, 101, 102, 103, 104,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 62, 63, 64, 64, 64, 64, 64, 65, 66, 67,
    68, 69, 70, 71, 13, 32, 34, 40, 22, 37, 20, 21, 27, 30, 85, 86,
    6, 8, 87, 1, 2, 3, 4, 5, 9, 10, 11, 15, 16, 17, 18, 19,
    95, 69, 69, 69, 69, 96, 0, 0, 73, 74, 75, 76, 77, 78, 79, 80,
    81, 82, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    23, 24, 25, 26, 28, 29, 31, 33, 35, 38, 41, 36, 42, 83, 39, 12,
    0, 0, 0, 0, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54,
    55, 56, 57, 58, 59, 60, 61, 84, 97, 98, 99, 100, 101, 102, 103, 104,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 62, 63, 90, 91, 92, 93, 94, 65, 66, 67,
    68, 69, 70, 71, 14, 32, 34, 40, 22, 37, 20, 21, 27, 30, 85, 86,
    7, 8, 87, 1, 2, 3, 4, 5, 9, 10, 11, 15, 16, 17, 18, 19,
};

// 0x40a520: choose a free visible tile next to (tx,ty) to walk to.
void ServantSetDest(Servant *s, float tx, float ty)
{
    int tries = 0;
    for (;;) {
        int cx = (int)(CosDeg(s->wanderAngle) * 0.25f + tx);
        int cy = (int)(ty - SinDeg(s->wanderAngle) * 0.25f);
        if (VIS(cx, cy) < 0xfe) {
            int j;
            for (j = 0; j < 10; j++) {
                Servant *o = &gServants[j];
                if (o->active == 1 && o->x != s->x && o->y != s->y &&
                    (float)cx == o->destX && (float)cy == o->destY)
                    break;
            }
            if (j == 10) {
                s->destX = (float)cx + 0.5f;
                s->destY = (float)cy + 0.5f;
                return;
            }
        }
        s->wanderAngle = rand() % 360;
        if (++tries >= 3) {
            int a = s->wanderAngle;
            s->destX = (float)(kRaw44b650[a] * 3) + tx;
            s->destY = (float)(kRaw44b650[a + 8] * 3) + ty;
            return;
        }
    }
}

// 0x40a650
void ServantHitTarget(Servant *s)
{
    if (StrikeAt(s->x, s->y, s->angle)) {
        s->model = s->anim.Loop(6);
        s->mode = 3;
        s->counter = 0;
    }
}

// 0x40a690: make every servant vanish; slot 'keep' is simply switched off.
void DismissServants(int keep)
{
    for (int i = 0; i < 10; i++) {
        Servant *s = &gServants[i];
        if (s->active != 1) continue;
        if (keep == i) {
            s->active = i;
        } else {
            s->model = s->anim.Loop(6);
            s->mode = 3;
            s->counter = i;
        }
    }
}

// 0x40a6e0
int CountServants()
{
    int n = 0;
    for (int i = 0; i < 10; i++)
        if (gServants[i].active == 1) n++;
    return n;
}

// 0x40a700: move servants along with the player to a new map position.
void ShiftServants(int oldX, int oldY, int newX, int newY)
{
    int dx = oldX - newX;
    int dy = oldY - newY;
    for (int i = 0; i < 10; i++) {
        Servant *s = &gServants[i];
        if (!s->active) continue;
        if (s->mode == 3) {
            s->active = 0;
            continue;
        }
        s->x = s->x - (float)dx;
        s->y = s->y - (float)dy;
        s->targetIdx = -1;
    }
}

// 0x40a760
void ServantDraw(Servant *s)
{
    FGObject o;
    memset(&o, 0, sizeof(o));
    if (s->x < 0.0f || !((float)gMapWidth > s->x)) return;
    if (s->y < 0.0f || !((float)gMapHeight > s->y)) return;
    float x = s->x, y = s->y;
    if (VIS((int)x, (int)y) >= 0xfe) return;
    WorldToScreen(x, y, &s->sx, &s->sy);
    s->sx += 0x20;
    s->sy += 0xf;
    o.type = 0x210;
    o.x0 = s->sx;
    o.y0 = s->sy;
    o.depth = ScreenDepth(s->x, s->y);
    if (s->mode == 3)
        o.light = -1;
    else {
        o.light = LIGHT((int)s->x, (int)s->y);
        if (gColorLight) o.lrgb = SmoothLightModel() ? LightRGBAt(s->x, s->y) : gLightRGB[(int)s->x * 65 + (int)s->y];
    }
    o.frame = s->anim.Advance();
    o.rect = nullptr;
    o.color = 0;
    o.model = s->model;
    o.angle = (float)(s->angle - 135);
    AddFGObject(&o);
}

// 0x40a8d0
void ServantVanish(Servant *s)
{
    if (++s->counter == 3) PlaySound(0x52, (int)s->x, (int)s->y);
    if (s->counter > 4) GroundSpark(s->x, s->y);
    if (s->anim.AtEnd()) s->active = 0;
}

// ---------------------------------------------------------------------------
// Dirty rectangles: screen areas saved before drawing a popup and restored
// after the page flip (0x40a930-0x40aa20).
// ---------------------------------------------------------------------------

struct DirtyRect {
    int x, y;
    Sprite *saved;
};
static DirtyRect gDirtyRects[20];  // 0x4af9c0
static int gNumDirtyRects;         // 0x4afab4

// 0x40a930
void AddDirtyRect(int x, int y, int w, int h)
{
    DirtyRect *r = &gDirtyRects[gNumDirtyRects++];
    r->x = x;
    r->y = y;
    r->saved = GrabScreen(x, y, w, h);
}

// 0x40a970
void RestoreDirtyRects()
{
    for (int i = 0; i < gNumDirtyRects; i++)
        if (gDirtyRects[i].saved) RestoreScreen(gDirtyRects[i].x, gDirtyRects[i].y, gDirtyRects[i].saved);
    gNumDirtyRects = 0;
}

// 0x40a9c0
void UpdateAndRestore(DirectDrawWindow *d)
{
    d->UpdateScreen();
    RestoreDirtyRects();
}

// 0x40a9d0
void FreeDirtyRects()
{
    for (int i = 0; i < gNumDirtyRects; i++) {
        if (gDirtyRects[i].saved) delete gDirtyRects[i].saved;
        gDirtyRects[i].saved = nullptr;
    }
    gNumDirtyRects = 0;
}
