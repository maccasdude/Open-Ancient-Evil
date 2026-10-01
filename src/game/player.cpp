// The player: action state machine, movement, jumping, casting animation,
// camera following and drawing (0x421f10-0x4244f0).
#include "game.h"
#include "../platform/platform.h"

// (port: held keys go through the key bindings, settings.cpp)

static void SetAnim(int a) { gPlayerModel = gPlayerAnim.Loop(a); }

static void Stop()
{
    gPlayer.gameMode = 0;
    SetAnim(0);
    gPlayer.actionFrame = 0;
}
void PlayerStop() { Stop(); }

// walking pace from the shift key / "always run"
static int PaceAnim()
{
    if (!ActionHeld(ACT_RUN) && !gPrefs.alwaysRun) {
        gPlayer.speed = gWalkSpeed;
        return 4;
    }
    gPlayer.speed = gRunSpeed;
    return 5;
}

// 0x421f10
void UpdatePlayer()
{
    if (gUIMode) return;
    UpdateLightRadius();
    ControlsUpdate();   // port: Diablo / WASD control schemes
    if (gPlayer.bowCooldown) gPlayer.bowCooldown--;
    switch (gPlayer.gameMode) {
    case 0: PlayerIdle(); break;
    case 1: PlayerAttack(); break;
    case 2:
        PlayerWalk();
        if (!g_5c5824) ComputeLighting();
        break;
    case 3: PlayerShoot(); break;
    case 4: PlayerJumpCharge(); break;
    case 5: PlayerJump(); break;
    case 6: PlayerCast(); break;
    case 7: PlayerPlayMusic(); break;
    case 8: PlayerHurt(); break;
    case 9: PlayerCastWindup(); break;
    case 12: gPlayer.actionFrame++; break;
    case 15: PlayerTeleport(); break;
    case 16: PlayerTurnLeft(); break;
    case 17: PlayerTurnRight(); break;
    case 18: PlayerBackstep(); break;
    case 19: PlayerSwitchWeapon(); break;
    }
    gPlayer.tileX = (int)gPlayer.x;
    gPlayer.tileY = (int)gPlayer.y;
    ControlsCamera();   // port: the Diablo / WASD camera follows the player
}

static void AheadTarget(float mul)
{
    gPlayer.destX = SinDeg(gPlayer.angle) * gWalkSpeed * mul + gPlayer.x;
    gPlayer.destY = gPlayer.y - CosDeg(gPlayer.angle) * gWalkSpeed * mul;
}

// 0x422040: standing; keyboard and mouse commands.
void PlayerIdle()
{
    if (ActionHeld(ACT_TURN_LEFT)) {
        gPlayer.angle -= 0xf;
        while (gPlayer.angle < 0) gPlayer.angle += 0x168;
        if (gPlayerAnim.curIndex == 0)
            gPlayerModel = gPlayerAnim.Once(0xd, &gPlayerModel, 0);
        else if (gPlayerAnim.curIndex == 1)
            gPlayerModel = gPlayerAnim.Once(0xf, &gPlayerModel, 1);
    } else if (ActionHeld(ACT_TURN_RIGHT)) {
        gPlayer.angle += 0xf;
        while (gPlayer.angle > 0x168) gPlayer.angle -= 0x168;
        if (gPlayerAnim.curIndex == 0)
            gPlayerModel = gPlayerAnim.Once(0xe, &gPlayerModel, 0);
        else if (gPlayerAnim.curIndex == 1)
            gPlayerModel = gPlayerAnim.Once(0xf, &gPlayerModel, 1);
    } else if (ActionHeld(ACT_WALK_FORWARD)) {
        AheadTarget(4.0f);
        gPlayer.speed = 0.2f;
        if (CanWalk()) {
            int a = PaceAnim();
            gPlayer.gameMode = 2;
            gPlayerModel = gPlayerAnim.Loop(a);
            gPlayer.actionFrame = 0;
        }
    } else if (ActionHeld(ACT_WALK_BACK)) {
        AheadTarget(-4.0f);
        gPlayer.speed = -gWalkSpeed;
        if (CanWalk()) {
            gPlayer.gameMode = 0x12;
            gPlayerModel = gPlayerAnim.ReverseLoop(4);
            gPlayer.actionFrame = 0;
        }
    }

    gPathActive = 0;
    if (g_5ad908 == 0 && gLeftClick) {
        if (!ActionAny(ACT_FACE)) {
            if (!gItemHover) {
                if (gDoorHover != -1) {
                    UseDoor(gDoorHover);
                    return;
                }
                if (gFeatureHover != -1) {
                    UseFeature(gFeatureHover);
                    return;
                }
            } else if (PickUpItems()) {
                return;
            }
        }
        float wx, wy;
        ScreenToWorld(gMouseX, gMouseY, &wx, &wy);
        int a = AngleBetween(gPlayer.x, gPlayer.y, wx, wy);
        if (ActionAny(ACT_FACE)) {
            gPlayer.turnAngle = a;
            StartTurn();
            return;
        }
        if (SetWalkTarget()) {
            int anim = PaceAnim();
            if (CanWalk()) {
                gPlayer.gameMode = 2;
                gPlayerModel = gPlayerAnim.Loop(anim);
                gPlayer.actionFrame = 0;
                return;
            }
        }
    } else if (gPlayerAnim.curIndex == 1 && gPlayerAnim.frame == 1 && rand() % 4 == 2 &&
               gPlayer.jumpCharge == 0) {
        // idle fidget
        gPlayerModel = gPlayerAnim.Once(rand() % 2 + 2, &gPlayerModel, 1);
    }

    if (gAttackRequest) {
        gAttackRequest = 0;
        switch (gActionMode) {
        case 0:
            if (gEquip.weaponType == 4) {
                if (!gPlayer.bowCooldown) ShootBow();
            } else {
                MeleeAttack();
            }
            break;
        case 2:
            if (CanMoveTo(&gPlayer, 0.5f)) {
                if (gPlayer.p60 != -1) {
                    gPlayer.gameMode = 4;
                    gPlayer.jumpCharge = 0;
                }
            } else {
                ShowMessage(gMsg[95] /* No room to jump */, gColorWhite);
            }
            break;
        }
    }
    gFootstepTimer = 0;
    gPlayer.animFrame = 0;
    ScrollCamera(gPlayer.x, gPlayer.y);
}

// 0x4224c0
void MeleeAttack()
{
    int m = MonsterUnderMouse();
    if (m != -1) {
        int a = AngleToMonster(m);
        gPlayer.turnAngle = a;
        int d = abs(a - gPlayer.angle);
        if (d > 0x1e && d < 0x14a) {
            StartTurn();
            gPlayer.pendingAction = 0x28;
            return;
        }
    }
    gPlayer.gameMode = 1;
    gPlayer.actionFrame = 0;
}

// 0x422520
void ShootBow()
{
    int m = MonsterUnderMouse();
    if (m != -1) {
        int a = AngleToMonster(m);
        gPlayer.turnAngle = a;
        int d = abs(a - gPlayer.angle);
        if (d > 0x1e && d < 0x14a) {
            StartTurn();
            gPlayer.pendingAction = 0x50;
            return;
        }
    }
    gPlayer.gameMode = 3;
    gPlayer.actionFrame = 0;
    gPlayerModel = gPlayerAnim.Loop(6);
}

// 0x422590: walk target from the mouse (doors: stand in front of them).
int SetWalkTarget()
{
    if (ActionAny(ACT_FACE)) return 0;
    int vd = DoorUnderMouse();
    if (vd != -1) {
        Door &d = gDoors[gVisDoors[vd].door];
        if (d.orient == 1) {
            gPlayer.destX = (float)d.x + 0.5f;
            if ((float)d.y > gPlayer.y)
                gPlayer.destY = (float)d.y - 0.5f;
            else
                gPlayer.destY = (float)d.y + 1.5f;
        } else {
            if (gPlayer.x < (float)d.x)
                gPlayer.destX = (float)d.x - 0.5f;
            else
                gPlayer.destX = (float)d.x + 1.5f;
            gPlayer.destY = (float)d.y + 0.5f;
        }
    } else {
        ScreenToWorld(gMouseX, gMouseY, &gPlayer.destX, &gPlayer.destY);
    }
    int tx = (int)gPlayer.destX, ty = (int)gPlayer.destY;
    if (tx > 0 && ty > 0 && tx < gMapWidth && ty < gMapHeight && gLevelMap[gMapRow[ty] + tx] < 0x14) {
        gPathActive = PlanPath(gPlayer.x, gPlayer.y, gPlayer.angle, gPlayer.destX, gPlayer.destY);
        if (gPathActive && FollowPath(&gPlayer)) return 1;
    }
    gBusyCursor = 2;
    gPlayer.destX = gPlayer.x;
    gPlayer.destY = gPlayer.y;
    return 0;
}

static void StepTaken()
{
    ScrollCamera(gPlayer.x, gPlayer.y);
    CheckPlayerTile();
    gPlayer.actionFrame++;
    if (++gPlayer.animFrame >= 10) gPlayer.animFrame = 0;
    if (++gFootstepTimer == 5) {
        gFootstepTimer = 0;
        PlayFootstep();
    }
    gPlayer.height = TileHeight((int)gPlayer.x, (int)gPlayer.y);
}

static void KeyTurn()
{
    if (ActionHeld(ACT_TURN_LEFT)) {
        gPlayer.angle -= 0x14;
        if (gPlayer.angle < 0) gPlayer.angle += 0x168;
    } else if (ActionHeld(ACT_TURN_RIGHT)) {
        gPlayer.angle += 0x14;
        if (gPlayer.angle >= 0x168) gPlayer.angle -= 0x168;
    }
}

// 0x4227a0: walking forwards.
void PlayerWalk()
{
    if (gStats.charClass != 3) gPlayer.noise++;
    if (gAttackRequest == 0) {
        float dx = SinDeg(gPlayer.angle) * gPlayer.speed;
        float dy = CosDeg(gPlayer.angle) * gPlayer.speed;
        gPlayer.x = dx + gPlayer.x;
        gPlayer.y = gPlayer.y - dy;
        if (gLevelMap[gMapRow[(int)gPlayer.y] + (int)gPlayer.x] < 0x14) {
            StepTaken();
            if (gLeftClick && gMouseY < 0x190) {
                gNewWalkTarget = 1;
                gPathActive = 0;
            }
            if (ActionHeld(ACT_WALK_FORWARD)) {
                KeyTurn();
                if (!ActionHeld(ACT_RUN) && !gPrefs.alwaysRun) {
                    if (gPlayerAnim.curIndex == 5) {
                        gPlayerModel = gPlayerAnim.LoopFrom(4, gPlayerAnim.frame);
                        gPlayer.speed = gWalkSpeed;
                    }
                } else if (gPlayerAnim.curIndex == 4) {
                    gPlayerModel = gPlayerAnim.LoopFrom(5, gPlayerAnim.frame);
                    gPlayer.speed = gRunSpeed;
                }
                if (!CanWalk()) {
                    gPlayer.gameMode = 0;
                    SetAnim(0);
                    gNewWalkTarget = 0;
                    gPlayer.actionFrame = 0;
                    return;
                }
                gPlayer.destX = SinDeg(gPlayer.angle) * gPlayer.speed * 4.0f + gPlayer.x;
                gPlayer.gameMode = 2;
                gNewWalkTarget = 0;
                gPlayer.actionFrame = 0;
                gPlayer.destY = gPlayer.y - CosDeg(gPlayer.angle) * gPlayer.speed * 4.0f;
                return;
            }
            if (gPlayer.actionFrame != gPlayerAnim.numFrames && gPlayer.actionFrame % 3 != 0) return;
            bool keepGoing;
            if (gNewWalkTarget) {
                if (SetWalkTarget()) {
                    if (!ActionHeld(ACT_RUN) && !gPrefs.alwaysRun) {
                        if (gPlayerAnim.curIndex == 5) SetAnim(4);
                        gNewWalkTarget = 0;
                        gPlayer.speed = gWalkSpeed;
                    } else {
                        if (gPlayerAnim.curIndex == 4) SetAnim(5);
                        gNewWalkTarget = 0;
                        gPlayer.speed = gRunSpeed;
                    }
                    gPlayer.actionFrame = 0;
                    return;
                }
                keepGoing = false;
            } else if (gPathActive) {
                keepGoing = FollowPath(&gPlayer) && CanWalk();
            } else {
                float d = DistanceB(gPlayer.x, gPlayer.y, gPlayer.destX, gPlayer.destY);
                keepGoing = !(gPlayer.speed + gPlayer.speed > d) && CanWalk();
            }
            if (!keepGoing) {
                SetAnim(0);
                gPlayer.gameMode = 0;
                gPathActive = 0;
            }
            gNewWalkTarget = 0;
            gPlayer.actionFrame = 0;
            return;
        }
        // walked into a wall: step back
        gPlayer.x = gPlayer.x - dx;
        gPlayer.y = dy + gPlayer.y;
    }
    gAttackRequest = 0;
    gPlayer.gameMode = 0;
    SetAnim(0);
    gPlayer.actionFrame = 0;
}

// 0x422bd0: walking backwards (down arrow).
void PlayerBackstep()
{
    if (gStats.charClass != 3) gPlayer.noise++;
    if (!CanWalk() || gAttackRequest) {
        gAttackRequest = 0;
        gPlayer.gameMode = 0;
        SetAnim(0);
        gPlayer.actionFrame = 0;
        return;
    }
    float dx = SinDeg(gPlayer.angle) * gPlayer.speed;
    float dy = CosDeg(gPlayer.angle) * gPlayer.speed;
    gPlayer.x = dx + gPlayer.x;
    gPlayer.y = gPlayer.y - dy;
    StepTaken();
    if (ActionHeld(ACT_WALK_BACK)) {
        KeyTurn();
        if (!CanWalk()) {
            gPlayer.gameMode = 0;
            SetAnim(0);
            gPlayer.actionFrame = 0;
            return;
        }
        gPlayer.destX = SinDeg(gPlayer.angle) * gPlayer.speed * 4.0f + gPlayer.x;
        gPlayer.gameMode = 0x12;
        gPlayer.actionFrame = 0;
        gPlayer.destY = gPlayer.y - CosDeg(gPlayer.angle) * gPlayer.speed * 4.0f;
        return;
    }
    if (gPlayer.actionFrame != gPlayerAnim.numFrames && gPlayer.actionFrame % 3 != 0) return;
    float d = DistanceB(gPlayer.x, gPlayer.y, gPlayer.destX, gPlayer.destY);
    float s = fabsf(gPlayer.speed);
    if (!(s + s > d) && CanWalk()) {
        gPlayer.actionFrame = 0;
        return;
    }
    Stop();
}

// 0x422e40: melee attack animation.
void PlayerAttack()
{
    if (gPlayer.actionFrame == 0) {
        gPlayer.fatigue += 10;
        gPlayer.attacking = 1;
        int a;
        if (FindFightTarget()) {
            switch (gEquip.weaponType) {
            case 0: case 4: case 13: case 14: a = 0x10; break;
            default: a = 0x11; break;
            }
        } else {
            a = rand() % 3 + 6;
        }
        SetAnim(a);
    } else if (gPlayer.actionFrame == 4) {
        PlayerStrike();
        BreakWall();
        gAttackRequest = 0;
    }
    gPlayer.actionFrame++;
    if (gPlayerAnim.AtEnd()) {
        gPlayer.gameMode = 0;
        SetAnim(1);
        gPlayer.actionFrame = 0;
        gPlayer.attacking = 0;
        ResetMouseClicks();
        gAttackRequest = 0;
        return;
    }
    if (gPlayer.actionFrame > 6 && gAttackRequest) MeleeAttack();
}

// 0x422f50: sparkles before the casting animation.
void PlayerCastWindup()
{
    if (gPlayer.castSpell == -1) {
        gPlayer.gameMode = 0;
        return;
    }
    if (--gPlayer.actionFrame > 2) Splash(gPlayer.x, gPlayer.y, gSpellSparkle[gPlayer.castSpell]);
    if (gPlayer.actionFrame <= 0) {
        gPlayer.gameMode = 6;
        gPlayer.actionFrame = 0;
        FreePlayerModel();
        LoadPlayerModel();
        gPlayerModel = gPlayerAnim.Hold(gSpellAnim[gPlayer.castSpell]);
    }
}

static void FinishCast()
{
    gTarget = -1;
    DrawActionIcon(gHudVisible);
    gPlayer.gameMode = 0;
    ResetMouseClicks();
    gAttackRequest = 0;
    FreePlayerModel();
    LoadPlayerModel();
    SetAnim(0);
    gPlayer.actionFrame = 0;
    gPlayer.castSpell = -1;
    gPlayer.fxScale = 0.0f;
}

// 0x422fe0: the casting animation; the spell takes effect on its frame.
void PlayerCast()
{
    if (gPlayer.castSpell == -1) {
        gPlayer.gameMode = 0;
        return;
    }
    gPlayer.actionFrame++;
    LightningAroundPlayer();
    int s = gPlayer.castSpell;
    int f = gPlayer.actionFrame;
    if (f == gCastEnd[s]) {
        FinishCast();
        return;
    }
    if (f == gCastSoundFrame[s]) {
        PlaySound(gCastSound[s], -1, -1);
        f = gPlayer.actionFrame;
        s = gPlayer.castSpell;
    }
    if (f == gCastFireFrame[s]) {
        if (!CompleteCast(s)) {
            gLocFxActive = 0;
            FinishCast();
            return;
        }
        f = gPlayer.actionFrame;
        s = gPlayer.castSpell;
    }
    switch (s) {
    case 11:
        gPlayer.fxScale = gFearScale[f];
        break;
    case 19:
        if (f == 3) gLocFxActive = 1;
        break;
    case 28:
        if (f == 0xe) gLocFxActive = 1;
        break;
    case 29:
        if (f == 0x13) {
            Monster *m = GetMonster(gCurLevel, gTarget);
            StartLocationEffect(m->x, m->y, 2);
            gLocFxActive = 1;
            LightningArc(m->x, m->y);
            Smoke(m->x, m->y, 7);
            for (int a = 0; a < 0x168; a += 0x5a) BloodSplat(m->x, m->y, a, m->stats->bloodType);
            gAltView = 0xf;
        }
        break;
    }
}

// 0x423200: bow animation.
void PlayerShoot()
{
    gPlayer.actionFrame++;
    if (!gPlayerAnim.AtEnd()) {
        if (gPlayer.actionFrame != 3) return;
        if (FireArrow() && gPlayer.arrows) return;
    }
    gPlayer.gameMode = 0;
    SetAnim(1);
    gPlayer.actionFrame = 0;
}

// 0x423260: holding the button builds up the jump.
void PlayerJumpCharge()
{
    int r = MouseRepeat(&gPlayer.jumpCharge);
    if (r == 2) {
        if (gPlayer.jumpCharge > 0x1e) {
            gPlayer.gameMode = 5;
            gPlayer.actionFrame = 0;
            gPlayer.jumpDist = (float)gPlayer.jumpCharge * 0.01f * 3.0f;
            gPlayer.jumpSteps = (int)(gPlayer.jumpDist * 4.0f);
            gPlayer.jumpStep = 0;
            gPlayer.jumpCharge = -1;
            LoadPlayerModel();
            gPlayerModel = gPlayerAnim.Hold(0);
            return;
        }
    } else if (r != 3) {
        return;
    }
    gPlayer.jumpCharge = -1;
    gPlayer.gameMode = 0;
    gPlayer.actionFrame = 0;
}

// 0x423300
void PlayerJump()
{
    switch (gPlayer.actionFrame) {
    case 0:
        if (gPlayerAnim.AtEnd()) {
            gPlayer.actionFrame++;
            gPlayerModel = gPlayerAnim.Hold(1);
        }
        return;
    case 1: {
        gPlayer.x = SinDeg(gPlayer.angle) * 0.25f + gPlayer.x;
        gPlayer.y = gPlayer.y - CosDeg(gPlayer.angle) * 0.25f;
        ScrollCamera(gPlayer.x, gPlayer.y);
        gPlayer.jumpDist = gPlayer.jumpDist - 0.25f;
        gPlayer.jumpStep++;
        if (gPlayer.jumpDist > 0.0f && CanMoveTo(&gPlayer, 0.25f)) return;
        // land
        gPlayer.actionFrame++;
        gPlayer.jumpStep = gPlayer.jumpSteps;
        SetAnim(2);
        gPlayer.tileX = (int)gPlayer.x;
        gPlayer.tileY = (int)gPlayer.y;
        int t = gLevelMap[gMapRow[gPlayer.tileY] + gPlayer.tileX];
        if (t != 1 && t != 6 && t != 7 && t != 8) PlaySound(0x3b, -1, -1);
        CheckPlayerTile();
        gPlayer.height = TileHeight((int)gPlayer.x, (int)gPlayer.y);
        return;
    }
    case 2:
        if (gPlayerAnim.AtEnd()) {
            gPlayer.gameMode = 0;
            gPlayer.actionFrame = 0;
            gAttackRequest = 0;
            LoadPlayerModel();
            SetAnim(0);
        }
        return;
    }
}

// 0x4234d0: playing an instrument.
void PlayerPlayMusic()
{
    switch (gPlayer.actionFrame) {
    case 0:
        if (gPlayerAnim.AtEnd()) {
            SetAnim(1);
            gPlayer.actionFrame++;
            gPlayer.restTime = 0;
            gMusicSound = rand() % 5 + 0x44;
            PlaySound(gMusicSound, -1, -1);
        }
        return;
    case 1:
        gPlayer.restTime++;
        if (gLeftClick) StopSound(gMusicSound);
        if (!SoundFinished(gMusicSound)) return;
        gPlayer.actionFrame++;
        SetAnim(2);
        // playing long enough in the right spot of level 21 reveals a reward
        if (gCurLevel == 0x15 && !(gPlayer.x < 8.0f) && !(gPlayer.x > 12.0f) && !(gPlayer.y < 32.0f) &&
            !(gPlayer.y > 37.0f) && gPlayer.restTime > 0x3c && !g_5bc194) {
            DropItem(10, 0x22, 0x98, 0);
            SetFeatureSprite(10, 0x21, 0x10a);
            g_5bc194++;
        }
        return;
    case 2:
        if (gPlayerAnim.AtEnd()) {
            gPlayer.gameMode = 0;
            gPlayer.actionFrame = 0;
            FreePlayerModel();
            LoadPlayerModel();
            SetAnim(0);
            KeyClear();
            ResetMouseClicks();
            gLeftClick = 0;
            gAttackRequest = 0;
        }
        return;
    }
}

// 0x4236b0: hit reaction.
void PlayerHurt()
{
    gPlayer.actionFrame++;
    if (gPlayerAnim.AtEnd()) {
        SetAnim(0);
        gPlayer.gameMode = 0;
    }
}

// 0x4236f0
void PlayerTeleport()
{
    int f = ++gPlayer.actionFrame;
    GroundSpark(gPlayer.x, gPlayer.y);
    f = gPlayer.actionFrame;
    if (f > 0xb) {
        gPlayer.height -= 0xc;
        if (gPlayer.height < 0) gPlayer.height = 0;
    } else if (f < 9) {
        gPlayer.height += 0xc;
    }
    if (f == 0xa) {
        gPlayer.tileX = gPlayer.teleX;
        gPlayer.tileY = gPlayer.teleY;
        gPlayer.x = gCamX = (float)gPlayer.teleX + 0.5f;
        gPlayer.y = gCamY = (float)gPlayer.teleY + 0.5f;
        FreePlayerModel();
        LoadPlayerModel();
        SetAnim(0);
        PlaySound(0x52, gPlayer.tileX, gPlayer.tileY);
        g_5c5824 = 1;
        return;
    }
    if (f == 0x12) {
        gPlayer.gameMode = 0;
        gPlayer.actionFrame = 0;
        gPlayer.height = 0;
    }
}

static void TurnDone()
{
    gPlayer.angle = gPlayer.turnAngle;
    if (gPlayer.pendingAction == -1) {
        gPlayer.gameMode = 0;
        SetAnim(gPlayerAnim.curIndex == 0xf ? 1 : 0);
    } else {
        DoPendingAction();
    }
}

// 0x4237f0
void PlayerTurnRight()
{
    int d = abs(gPlayer.turnAngle - gPlayer.angle);
    if (d >= 0x1e && d <= 0x14a) {
        gPlayer.angle = (gPlayer.angle + 0x1e) % 0x168;
        return;
    }
    TurnDone();
    gPlayer.angle %= 0x168;
}

// 0x4238a0
void PlayerTurnLeft()
{
    int d = abs(gPlayer.turnAngle - gPlayer.angle);
    if (d >= 0x1e && d <= 0x14a)
        gPlayer.angle -= 0x1e;
    else
        TurnDone();
    if (gPlayer.angle < 0) gPlayer.angle += 0x168;
}

// 0x423930
void StartTurn()
{
    int d = gPlayer.turnAngle - gPlayer.angle;
    gPlayer.actionFrame = 0;
    if (d == 0) return;
    if ((d > 0 && d < 0xb4) || d < -0xb4) {
        gPlayer.gameMode = 0x11;
        SetAnim(gPlayerAnim.curIndex ? 0xf : 0xe);
    } else {
        gPlayer.gameMode = 0x10;
        SetAnim(gPlayerAnim.curIndex ? 0xf : 0xd);
    }
}

// 0x4239c0: can the player move on? Tries small detours to either side.
int CanWalk()
{
    int orig = gPlayer.angle;
    if (CanMoveTo(&gPlayer, gPlayer.speed)) return 1;
    int off = 5;
    for (int i = 0; i < 8; i++) {
        int a = off + orig;
        if (a < 0) a += 0x168;
        if (a >= 0x168) a -= 0x168;
        gPlayer.angle = a;
        if (CanMoveTo(&gPlayer, gPlayer.speed)) return 1;
        off = off < 0 ? abs(off) + 5 : -off;
    }
    gPlayer.angle = orig;
    return 0;
}

// 0x423a60
void DoPendingAction()
{
    switch (gPlayer.pendingAction) {
    case 0x28:
        gPlayer.gameMode = 1;
        gPlayer.actionFrame = 0;
        break;
    case 0x36: BeginCast(4); break;
    case 0x37: BeginCast(5); break;
    case 0x3c: BeginCast(10); break;
    case 0x50:
        gPlayer.gameMode = 3;
        gPlayer.actionFrame = 0;
        SetAnim(6);
        break;
    }
    gPlayer.pendingAction = -1;
}

// 0x423b60: probes ahead of the player for walls and blocking monsters.
int CanMoveTo(Player *p, float dist)
{
    float X = 0, Y = 0;
    for (int i = 0; i < 3; i++) {
        const Vec3 &pr = gPlayerProbes[i];
        Vec3 v = {pr.x * dist, pr.y, pr.z * dist};
        RotateY(&v, &v, -(float)p->angle);
        X = p->x + v.x;
        Y = v.z + p->y;
        if (gLevelMap[gMapRow[(int)Y] + (int)X] >= 0x14) return 0;
    }
    for (int i = 0; i < 50; i++) {
        Monster *m = GetMonster(gCurLevel, i);
        if (gMapVis[m->tileX * 65 + m->tileY] == 0xff) continue;
        if (m->state == 0x80) continue;
        if (!m->stats || !m->stats->blocks) continue;
        if (CircleOverlap(X, Y, 0.5f, m->x, m->y, m->radius)) return 0;
    }
    return 1;
}

// 0x423c90
void DamagePlayer(int dmg, int bleed)
{
    if (gGodMode || gPlayer.forceField || gPlayer.gameMode == 0xc) return;
    gStats.hp -= dmg;
    if (gPlayer.gameMode != 7) {
        int s = gStats.charClass == 3 ? rand() % 5 + 0x64 : rand() % 5 + 0x1b;
        PlaySound(s, -1, -1);
    }
    if (bleed) BloodSplat(gPlayer.x, gPlayer.y, rand() % 0x167, 2);
    if (gStats.hp < gStats.maxHp / 3) ShowMessage(gMsg[269] /* You are almost dead... */, gColorRed);
}

// 0x423d70
void PlayerKnockback(int behind)
{
    if (gPlayer.forceField) return;
    int m = gPlayer.gameMode;
    if ((m == 0 && gPlayerAnim.curIndex == 0) || m == 6 || m == 9) {
        gPlayer.gameMode = 8;
        gPlayer.actionFrame = 0;
        int a = behind ? 10 : 9;
        if (m == 6 || m == 9) {
            FreePlayerModel();
            LoadPlayerModel();
            gTarget = -1;
        }
        SetAnim(a);
    }
    gPlayer.jumpCharge = -1;
}

// 0x423e00 (compares with r0^2 + r1^2)
int CircleOverlap(float x0, float y0, float r0, float x1, float y1, float r1)
{
    float d = DistanceB(x0, y0, x1, y1);
    return d < (double)r0 * r0 + (double)r1 * r1;
}

// 0x423e50: mouse edge scrolling and keeping the player near the centre.
void ScrollCamera(float x, float y)
{
    if (gSettings.scheme != kSchemeClassic && gSettings.followCamera) return;   // (port: ControlsCamera)
    if (gPlayer.gameMode == 0) {
        float dx = gCamX - x, dy = gCamY - y;
        if (gMouseX == ViewL()) {
            if (dx > -3.0f) gCamX = gCamX - 0.5f;
            if (dy < 2.5f) gCamY = gCamY + 0.5f;
            g_5c5824 = 1;
        } else if (gMouseX == ViewR()) {
            if (dx < 3.0f) gCamX = gCamX + 0.5f;
            if (dy > -2.5f) gCamY = gCamY - 0.5f;
            g_5c5824 = 1;
        }
        if (gMouseY == ViewT()) {
            if (dy > -2.5f) gCamY = gCamY - 0.5f;
            if (dx > -3.0f) gCamX = gCamX - 0.5f;
            g_5c5824 = 1;
        } else if (gMouseY == 0x1df) {
            if (dy < 2.5f) gCamY = gCamY + 0.5f;
            if (dx < 3.0f) gCamX = gCamX + 0.5f;
            g_5c5824 = 1;
        }
    }
    float ax = gCamX - x;
    float ay = gCamY - y;
    if (ax > 3.0f) {
        g_5c5824 = 1;
        gCamX = gCamX - (ax - 3.0f);
    } else if (ax < -3.0f) {
        g_5c5824 = 1;
        gCamX = fabsf(ax) - 3.0f + gCamX;
    }
    if (ay > 2.5f) {
        g_5c5824 = 1;
        gCamY = gCamY - (ay - 2.5f);
    } else if (ay < -2.5f) {
        g_5c5824 = 1;
        gCamY = fabsf(ay) - 2.5f + gCamY;
    }
}

// 0x424070: putting the weapon away / drawing it.
void PlayerSwitchWeapon()
{
    if (gPlayer.actionFrame == 0) {
        DrawActionIcon(gHudVisible);
        gPlayer.actionFrame++;
        if (gPrevActionMode == 0 && gEquip.weaponType != 0) {
            SetAnim(0x13);
            return;
        }
        if (gActionMode == 0 && gEquip.weaponType != 0) {
            FreePlayerModel();
            LoadPlayerModel();
            SetAnim(0x12);
            return;
        }
    } else {
        gPlayer.actionFrame++;
        if (gActionMode == 0 && gPlayerAnim.AtEnd()) {
            SetAnim(1);
            UpdateLightRadius();
            gPlayer.gameMode = 0;
            gPlayer.actionFrame = 0;
        }
        if (gPrevActionMode != 0 || !gPlayerAnim.AtEnd()) return;
    }
    UpdateLightRadius();
    FreePlayerModel();
    LoadPlayerModel();
    if (g_5ad910 == 0) {
        gPlayer.gameMode = 0;
        gPlayer.actionFrame = 0;
        return;
    }
    BeginCast(gPlayer.currentSpell);
    g_5ad910 = 0;
}

// 0x424180: special floor tiles under the player.
void CheckPlayerTile()
{
    int x = (int)gPlayer.x;
    gPlayer.tileX = x;
    int y = (int)gPlayer.y;
    gPlayer.tileY = y;
    switch (gLevelMap[gMapRow[y] + x]) {
    case 4:
        if (gPlayer.gameMode != 5) PressPlate(x, y);
        break;
    case 0x10:
        if (gPlayer.gameMode != 5) TakeStairs(x, y);
        break;
    case 0x11:
        AddBurner(x, y);
        break;
    }
}

// 0x424200
void TakeStairs(int x, int y)
{
    gDDW.FillRect(0, 0, 0x140, 0xa0, 0);
    gDDW.UpdateScreen();
    LeaveGame();
    ChangeLevelAt(x, y);
    EnterLevel(gCurLevel);
    gPlayer.actionFrame = 0;
    gPlayer.gameMode = 0;
}

// 0x424260
void DrawPlayer()
{
    if (gUIMode) {
        DrawPlayerModel(gPlayer.actionFrame);
        return;
    }
    if (gPlayer.gameMode == 2 || gPlayer.gameMode == 5)
        DrawPlayerModel(gPlayer.actionFrame);
    else
        DrawPlayerModel(0);
}

// 0x4242b0
void DrawPlayerModel(int)
{
    gPlayer.animFrame = gPlayerAnim.Advance();
    WorldToScreen(gPlayer.x, gPlayer.y, &gPlayer.sx, &gPlayer.sy);
    gPlayer.sx += 0x1f;
    if (gPlayer.jumpSteps) {
        float t = SinF((float)gPlayer.jumpStep / (float)gPlayer.jumpSteps * 180.0f);
        gPlayer.sy += -2 * (int)(t * (float)gPlayer.jumpSteps);
    }
    ShieldJitter();
    FGObject o;
    memset(&o, 0, sizeof o);
    float depth = ScreenDepth(gPlayer.x, gPlayer.y);
    o.color = gPlayer.height;
    o.x0 = gPlayer.sx;
    o.y0 = gPlayer.sy + gPlayer.height;
    o.frame = gPlayer.animFrame;
    o.model = gPlayerModel;
    o.fx = gPlayer.x;
    o.fy = gPlayer.y;
    o.scale = gPlayer.fxScale;
    gPlayer.depth = depth;
    o.depth = depth;
    o.angle = (float)(gPlayer.angle - 0x87);
    o.type = 0x10;
    o.light = gLightMap[gPlayer.tileX * 65 + gPlayer.tileY];
    if (gColorLight) o.lrgb = SmoothLightModel() ? LightRGBAt(gPlayer.x, gPlayer.y) : gLightRGB[gPlayer.tileX * 65 + gPlayer.tileY];
    if (gPlayer.invisibility) o.type = 0x210;
    o.rect = (RECT *)gPlayer.rect;
    AddFGObject(&o);
    if (gPlayer.gameMode != 0xf && !gPlayer.invisibility)
        DrawModelShadow(gPlayer.x, gPlayer.y, gPlayerModel, o.frame, gPlayer.angle, 0);
    RECT *r = (RECT *)gPlayer.rect;
    r->left = -9999;
    r->top = -9999;
    r->right = -9990;
    r->bottom = -9990;
    if (gLightSpell) DrawLightSpellAt(gPlayer.sx, gPlayer.sy);
}

// 0x4244a0: the magic shield makes the player shimmer.
void ShieldJitter()
{
    if (!gPlayer.magicShield) return;
    gPlayer.sx = rand() % 5 + gPlayer.sx - 2;
    gPlayer.sy = rand() % 5 + gPlayer.sy - 2;
}

// ===========================================================================
// Port: the Diablo style and WASD control schemes (settings.h). The classic
// scheme is the original code above; the others steer the same state
// machine from here, at the start of UpdatePlayer.
// ===========================================================================

int gCtlRightClick;                    // right clicks for the scheme (GameMouse)
static int gCtlTarget = -1;            // Diablo: monster being chased
static int gCtlTargetTalk;             //   walking up to talk to it
static int gCtlUseKind, gCtlUseIdx;    //   thing to use on arrival: 1 feature, 2 item, 3 door
static bool gCtlHoldWalk;              //   left button held on the ground
static bool gCtlSteering;              // WASD: walking because of the keys
static bool gCtlJumpKey;               // jump charging from the jump key
static int gCtlRetarget;
static bool gCtlClick;                 // left button went down this frame (in the view)

// The original counts a click when the button comes up; the schemes act when
// it goes down (a quick click inside one frame still counts once).
static void ReadLeftPress()
{
    static bool prev, pressed;
    bool down = MouseButtonDown(0) != 0;
    gCtlClick = false;
    if (down && !prev) {
        pressed = true;
        gCtlClick = true;
    } else if (!down && gLeftClick && !pressed) {
        gCtlClick = true;
    }
    if (!down && !gLeftClick) pressed = false;
    if (!down && gLeftClick) pressed = false;
    prev = down;
}

bool ControlsOwnMouse() { return gSettings.scheme != kSchemeClassic; }

void ControlsReset()
{
    gCtlTarget = -1;
    gCtlUseKind = 0;
    gCtlHoldWalk = false;
    gCtlSteering = false;
    gCtlJumpKey = false;
    gCtlRightClick = 0;
}

bool ControlsJumpHeld() { return gCtlJumpKey && ActionHeld(ACT_JUMP); }

static bool MouseInView() { return gMouseY < gViewHeight && gMouseY >= ViewT(); }

static int CursorAngle()
{
    float wx, wy;
    ScreenToWorld(gMouseX, gMouseY, &wx, &wy);
    return AngleBetween(gPlayer.x, gPlayer.y, wx, wy);
}

static void FaceCursor()
{
    if (MouseInView()) gPlayer.angle = CursorAngle();
}

static void Attack()
{
    if (gEquip.weaponType == 4) {
        if (!gPlayer.bowCooldown) ShootBow();
    } else {
        MeleeAttack();
    }
}

static void CastAtCursor()
{
    if (gPlayer.gameMode != 0) return;
    FaceCursor();
    BeginCast(gPlayer.currentSpell);
}

static void StartJump()
{
    if (!CanMoveTo(&gPlayer, 0.5f)) {
        ShowMessage(gMsg[95] /* No room to jump */, gColorWhite);
        return;
    }
    if (gPlayer.p60 == -1) return;
    gPlayer.gameMode = 4;
    gPlayer.jumpCharge = 0;
    gCtlJumpKey = true;
}

// walk to a point along a planned path (as SetWalkTarget does for the mouse)
static bool WalkTo(float x, float y)
{
    int tx = (int)x, ty = (int)y;
    if (tx <= 0 || ty <= 0 || tx >= gMapWidth || ty >= gMapHeight || gLevelMap[gMapRow[ty] + tx] >= 0x14) return false;
    gPlayer.destX = x;
    gPlayer.destY = y;
    gPathActive = PlanPath(gPlayer.x, gPlayer.y, gPlayer.angle, x, y);
    if (!gPathActive || !FollowPath(&gPlayer)) {
        gPathActive = 0;
        gPlayer.destX = gPlayer.x;
        gPlayer.destY = gPlayer.y;
        return false;
    }
    int anim = PaceAnim();
    if (gPlayer.gameMode == 0) {
        if (!CanWalk()) return false;
        gPlayer.gameMode = 2;
        gPlayerModel = gPlayerAnim.Loop(anim);
        gPlayer.actionFrame = 0;
    }
    gNewWalkTarget = 0;
    return true;
}

// the walkable tile next to (tx,ty) nearest the player
static bool WalkBeside(int tx, int ty)
{
    float best = 1e9f, bx = 0, by = 0;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            int x = tx + dx, y = ty + dy;
            if ((!dx && !dy) || x <= 0 || y <= 0 || x >= gMapWidth || y >= gMapHeight) continue;
            if (gLevelMap[gMapRow[y] + x] >= 0x14) continue;
            float d = Distance(gPlayer.x, gPlayer.y, (float)x + 0.5f, (float)y + 0.5f);
            if (d < best) {
                best = d;
                bx = (float)x + 0.5f;
                by = (float)y + 0.5f;
            }
        }
    return best < 1e8f && WalkTo(bx, by);
}

static int ItemUnderMouseAny()
{
    for (int i = 0; i < gNumVisItems; i++) {
        VisItem &v = gVisItems[i];
        if (v.idx != -1 && MouseInRect((RECT *)v.rect)) return i;
    }
    return -1;
}

static int DoorUnderMouseAny()
{
    for (int i = 0; i < kMaxVisDoors; i++) {
        VisDoor &v = gVisDoors[i];
        if (v.active == 1 && MouseInRect((RECT *)v.rect)) return i;
    }
    return -1;
}

static int VisFeatureOf(int fi)
{
    for (int i = 0; i < kMaxVisFeatures; i++)
        if (gVisFeatures[i].active == 1 && gVisFeatures[i].feature == fi) return i;
    return -1;
}

static int VisItemOf(int li)
{
    for (int i = 0; i < gNumVisItems; i++)
        if (gVisItems[i].idx == li) return i;
    return -1;
}

static int VisDoorOf(int di)
{
    for (int i = 0; i < kMaxVisDoors; i++)
        if (gVisDoors[i].active == 1 && gVisDoors[i].door == di) return i;
    return -1;
}

// Diablo: arrived near the thing clicked? Then use it.
static void DiabloUse()
{
    if (!gCtlUseKind || (gPlayer.gameMode != 0 && gPlayer.gameMode != 2)) return;
    int kind = gCtlUseKind, idx = gCtlUseIdx;
    bool near = false;
    int vi = -1;
    switch (kind) {
    case 1:
        vi = VisFeatureOf(idx);
        near = vi >= 0 && FeatureInReach(vi);
        break;
    case 2:
        near = gLevelItems[idx].active == 1 && IsPickable(idx);
        vi = VisItemOf(idx);
        break;
    case 3:
        vi = VisDoorOf(idx);
        near = vi >= 0 && DoorReachable(idx);
        break;
    }
    if (!near) {
        if (gPlayer.gameMode == 0) gCtlUseKind = 0;   // stopped short: give up
        return;
    }
    gCtlUseKind = 0;
    if (gPlayer.gameMode == 2) Stop();
    if (kind == 1) {
        UseFeature(vi);
    } else if (kind == 3) {
        UseDoor(vi);
    } else if (vi >= 0) {
        // pick up through the original routine, with the cursor on the item
        int mx = gMouseX, my = gMouseY;
        VisItem &v = gVisItems[vi];
        gMouseX = (v.rect[0] + v.rect[2]) / 2;
        gMouseY = (v.rect[1] + v.rect[3]) / 2;
        PickUpItems();
        gMouseX = mx;
        gMouseY = my;
    }
}

static void DiabloChase()
{
    if (gCtlTarget < 0) return;
    Monster *m = GetMonster(gCurLevel, gCtlTarget);
    if (!m->IsActive() || (!gCtlTargetTalk && m->attitude)) {
        gCtlTarget = -1;
        return;
    }
    float d = Distance(gPlayer.x, gPlayer.y, m->x, m->y);
    const bool bow = gEquip.weaponType == 4 && !gCtlTargetTalk;
    const float reach = gCtlTargetTalk ? 2.0f : 1.4f;
    if (bow || d <= reach) {
        if (gPlayer.gameMode == 2) Stop();
        if (gPlayer.gameMode != 0) return;
        gPlayer.angle = AngleToMonster(gCtlTarget);
        if (gCtlTargetTalk) {
            gCtlTarget = -1;
            TalkToMonster(m - GetMonster(gCurLevel, 0));
            return;
        }
        Attack();
        if (!MouseButtonDown(0)) gCtlTarget = -1;   // held: keep fighting
        return;
    }
    if ((gPlayer.gameMode == 0 || gPlayer.gameMode == 2) && ++gCtlRetarget >= 4) {
        gCtlRetarget = 0;
        if (!WalkTo(m->x, m->y)) {
            float a = (float)AngleToMonster(gCtlTarget);
            WalkTo(gPlayer.x + SinDeg((int)a) * 0.8f, gPlayer.y - CosDeg((int)a) * 0.8f);
        }
    }
}

static void DiabloUpdate()
{
    bool click = gCtlClick && MouseInView();
    gLeftClick = 0;   // the scheme handles left clicks itself
    if (click) {
        gCtlUseKind = 0;
        gCtlTarget = -1;
        gCtlHoldWalk = false;
        int i;
        if (ActionHeld(ACT_ATTACK_IN_PLACE)) {
            if (gPlayer.gameMode == 2) Stop();
            if (gPlayer.gameMode == 0) {
                FaceCursor();
                Attack();
            }
        } else if ((i = MonsterUnderMouse()) != -1 && GetMonster(gCurLevel, i)->IsActive()) {
            gCtlTarget = i;
            gCtlTargetTalk = GetMonster(gCurLevel, i)->attitude != 0;
            gCtlRetarget = 99;
        } else if ((i = ItemUnderMouseAny()) != -1) {
            gCtlUseKind = 2;
            gCtlUseIdx = gVisItems[i].idx;
            LevelItem &it = gLevelItems[gCtlUseIdx];
            if (!IsPickable(gCtlUseIdx)) WalkTo((float)it.x + 0.5f, (float)it.y + 0.5f);
        } else if ((i = DoorUnderMouseAny()) != -1) {
            gCtlUseKind = 3;
            gCtlUseIdx = gVisDoors[i].door;
            if (!DoorReachable(gCtlUseIdx) && gPlayer.gameMode == 0) {
                gLeftClick = 1;   // the original walks to the front of the door
            }
        } else if ((i = FeatureUnderMouse()) != -1 && (FeatureClickable(i) || gFeatureNameIdx[gVisFeatures[i].sprite])) {
            gCtlUseKind = 1;
            gCtlUseIdx = gVisFeatures[i].feature;
            Feature &f = gFeatures[gCtlUseIdx];
            if (!FeatureInReach(i)) WalkBeside(f.x, f.y);
        } else if ((i = ObjectUnderMouse()) != -1) {
            gLeftClick = 1;   // (fountains: as the original)
        } else {
            gCtlHoldWalk = true;
            gCtlRetarget = 0;
            gLeftClick = 1;   // walk there, as the original
        }
    }
    if (gCtlHoldWalk) {
        if (!MouseButtonDown(0) || !MouseInView()) {
            gCtlHoldWalk = false;
        } else if (++gCtlRetarget >= 3) {
            // keep walking towards the cursor while the button is held
            gCtlRetarget = 0;
            if (gPlayer.gameMode == 0 || gPlayer.gameMode == 2) gLeftClick = 1;
        }
    }
    DiabloChase();
    DiabloUse();
    if (gCtlRightClick) {
        gCtlRightClick = 0;
        if (MouseInView()) {
            if (gPlayer.gameMode == 2) Stop();
            CastAtCursor();
        }
    }
}

static int gCtlAim = -1;   // WASD: last direction towards the cursor

// The direction towards the cursor, kept while the cursor is on the player.
static int CursorAim()
{
    float wx, wy;
    ScreenToWorld(gMouseX, gMouseY, &wx, &wy);
    if (gCtlAim < 0 || Distance(gPlayer.x, gPlayer.y, wx, wy) > 0.6f) gCtlAim = AngleBetween(gPlayer.x, gPlayer.y, wx, wy);
    return gCtlAim;
}

static int WrapAngle(int a)
{
    a %= 360;
    return a < 0 ? a + 360 : a;
}

// Walk (forward, or backwards facing 'face') along angle a; false if blocked.
static bool Steer(int a, bool backwards)
{
    const int mode = gPlayer.gameMode;
    int anim = backwards ? 4 : PaceAnim();
    if (backwards) gPlayer.speed = -gWalkSpeed;
    int old = gPlayer.angle;
    gPlayer.angle = a;
    // (well ahead: the walk routines compare a squared distance with the
    // step length, which stops the slow backward walk if this is close)
    gPlayer.destX = SinDeg(a) * gPlayer.speed * 12.0f + gPlayer.x;
    gPlayer.destY = gPlayer.y - CosDeg(a) * gPlayer.speed * 12.0f;
    gPathActive = 0;
    gNewWalkTarget = 0;
    if (!CanWalk()) {
        gPlayer.angle = old;
        return false;
    }
    const int want = backwards ? 0x12 : 2;
    if (mode != want || (!backwards && gPlayerAnim.curIndex != anim)) {
        gPlayerModel = backwards ? gPlayerAnim.ReverseLoop(4) : gPlayerAnim.Loop(anim);
        gPlayer.actionFrame = 0;
    }
    gPlayer.gameMode = want;
    return true;
}

static void WasdUpdate()
{
    const int fwd = (ActionHeld(ACT_MOVE_UP) ? 1 : 0) - (ActionHeld(ACT_MOVE_DOWN) ? 1 : 0);
    const int side = (ActionHeld(ACT_MOVE_RIGHT) ? 1 : 0) - (ActionHeld(ACT_MOVE_LEFT) ? 1 : 0);
    const int mode = gPlayer.gameMode;
    if ((fwd || side) && (mode == 0 || mode == 2 || mode == 0x12)) {
        bool ok;
        if (gSettings.wasdRelative) {
            // relative to the cursor: W towards it, S backs away from it
            // still facing it, A/D go round it
            int aim = MouseInView() ? CursorAim() : (gCtlAim < 0 ? gPlayer.angle : gCtlAim);
            if (fwd < 0)
                ok = Steer(WrapAngle(aim + side * 45), true);
            else
                ok = Steer(WrapAngle(aim + side * (fwd > 0 ? 45 : 90)), false);
        } else {
            // screen directions: (u,v) -> map: x - y = u/32, x + y = v/16 (WorldToScreen)
            float u = (float)side * 32.0f, v = (float)-fwd * 16.0f;
            if (side && fwd) v *= 2.0f;   // diagonals follow the screen diagonals
            float dx = (u / 32.0f + v / 16.0f) * 0.5f, dy = (v / 16.0f - u / 32.0f) * 0.5f;
            ok = Steer(AngleBetween(0.0f, 0.0f, dx, dy), false);
        }
        if (ok) {
            gCtlSteering = true;
        } else {
            if (gPlayer.gameMode == 2 || gPlayer.gameMode == 0x12) Stop();
            gCtlSteering = false;
        }
    } else if (gCtlSteering) {
        gCtlSteering = false;
        if (gPlayer.gameMode == 2 || gPlayer.gameMode == 0x12) Stop();
    }
    const bool click = gCtlClick && MouseInView();
    const bool held = MouseButtonDown(0) && MouseInView();
    // left button: use what is under the cursor, otherwise attack
    if (click && (gItemHover || gDoorHover != -1 || gFeatureHover != -1)) {
        gLeftClick = 1;   // (PlayerIdle picks up / opens / uses it)
    } else if (!click && gLeftClick && (gItemHover || gDoorHover != -1 || gFeatureHover != -1)) {
        gLeftClick = 0;   // (the release of a click already acted on)
    } else {
        gLeftClick = 0;
        if ((click || held) && gPlayer.gameMode == 0 && !gCtlSteering) {
            FaceCursor();
            Attack();
        }
    }
    // face the cursor while standing
    if (gPlayer.gameMode == 0 && !gCtlSteering) FaceCursor();
    if (gCtlRightClick) {
        gCtlRightClick = 0;
        if (MouseInView()) {
            if (gPlayer.gameMode == 2) Stop();
            gCtlSteering = false;
            CastAtCursor();
        }
    }
}

void ControlsUpdate()
{
    if (gSettings.scheme == kSchemeClassic || gUIMode) return;
    ReadLeftPress();
    if (gCtlJumpKey && gPlayer.gameMode != 4) gCtlJumpKey = false;
    // the jump key (Diablo, WASD): hold to charge, let go to leap
    if (ActionAny(ACT_JUMP) && !gCtlJumpKey && (gPlayer.gameMode == 0 || gPlayer.gameMode == 2)) {
        if (gPlayer.gameMode == 2) Stop();
        if (gSettings.scheme == kSchemeDiablo) FaceCursor();
        gCtlSteering = false;
        StartJump();
        return;
    }
    if (gSettings.scheme == kSchemeDiablo)
        DiabloUpdate();
    else
        WasdUpdate();
}

// Port: in the Diablo and WASD schemes the view stays centred on the
// player, easing after it (instead of the original's scrolling when the
// player nears the edge of a box around the centre).
void ControlsCamera()
{
    if (gSettings.scheme == kSchemeClassic || !gSettings.followCamera || gAltView) return;
    float dx = gPlayer.x - gCamX, dy = gPlayer.y - gCamY;
    if (fabsf(dx) < 0.01f && fabsf(dy) < 0.01f) return;
    if (fabsf(dx) > 12.0f || fabsf(dy) > 12.0f || fabsf(dx) + fabsf(dy) < 0.03f) {
        gCamX = gPlayer.x;   // (teleports: jump there; tiny steps: settle)
        gCamY = gPlayer.y;
    } else {
        gCamX += dx * 0.4f;
        gCamY += dy * 0.4f;
    }
    g_5c5824 = 1;
}
