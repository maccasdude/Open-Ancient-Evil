// Doors, secret walls, messages, entering and leaving levels, the player's
// model and textures, and story events (0x4244f0-0x425d10).
#include "game.h"

// 0x4244f0: doors opening and closing over 10 frames.
void UpdateDoors()
{
    for (int i = 0; i < 400; i++) {
        Door &d = gDoors[i];
        if (d.active != 1 || d.kind == -1) continue;
        if (d.open == 1) {
            if (++d.timer >= 10) {
                d.open = 2;
                SetMapCell(d.x, d.y, 3, 0);
                g_5c5824 = 1;
                FixWallTiles();
            }
        } else if (d.open == 3) {
            if (d.timer >= 10) SetMapCell(d.x, d.y, 3, 0x14);
            if (--d.timer <= 0) {
                SetMapCell(d.x, d.y, 3, d.orient == 0 ? 0x1b : 0x1a);
                FixWallTiles();
                d.open = 0;
                g_5c5824 = 1;
            }
        }
    }
}

// 0x4245d0: 'O'pen: try the doors ahead and diagonally ahead.
void OpenNearbyDoors()
{
    static const int kOff[3] = {-0x2d, 0, 0x2d};
    static const float kDist[3] = {1.414f, 1.0f, 1.414f};
    for (int i = 0; i < 3; i++) {
        int a = kOff[i] + gPlayer.angle;
        int tx = (int)(SinDeg(a) * kDist[i] + gPlayer.x);
        int ty = (int)(gPlayer.y - CosDeg(a) * kDist[i]);
        UseDoor(VisDoorAt(tx, ty));
    }
}

// 0x424670: open or close a visible door.
void UseDoor(int vi)
{
    if (vi < 0 || vi > 0x190) return;
    int di = gVisDoors[vi].door;
    if (!DoorReachable(di)) {
        ShowMessage(gMsg[2] /* It is too far away. */, gColorRed);
        return;
    }
    Door &d = gDoors[di];
    if (d.lock == 0x14) {
        ShowMessage(gMsg[4] /* This door is operated elsewhere. */, gColorWhite);
        return;
    }
    switch (d.open) {
    case 0:
        d.open = 1;
        PlaySound(gDoorSounds[d.kind], -1, -1);
        ShowMessage(gMsg[3] /* O.K. */, gColorWhite);
        gPlayer.turnAngle = AngleBetween(gPlayer.x, gPlayer.y, (float)d.x + 0.5f, (float)d.y + 0.5f);
        StartTurn();
        break;
    case 2:
        if (MonsterAtPoint((float)d.x, (float)d.y, 1.0f) != 0xff) {
            ShowMessage(gMsg[203] /* Door blocked by monster */, gColorRed);
            return;
        }
        d.open = 3;
        PlaySound(gDoorSounds[d.kind], -1, -1);
        ShowMessage(gMsg[3] /* O.K. */, gColorWhite);
        gPlayer.noise += 0x32;
        return;
    case 4:
        ShowMessage(gMsg[130] /* This door requires a key */, gColorWhite);
        return;
    }
    gPlayer.noise += 0x32;
}

// 0x424860: is the player standing next to the door?
int DoorReachable(int di)
{
    Door &d = gDoors[di];
    if (d.level != gCurLevel + 1) return 0;
    int x = d.x, y = d.y;
    if (d.orient == 1) {
        if (x != gPlayer.tileX) return 0;
        return y - 1 == gPlayer.tileY || y + 1 == gPlayer.tileY;
    }
    if (x - 1 != gPlayer.tileX && x + 1 != gPlayer.tileX) return 0;
    return abs(y - gPlayer.tileY) <= 1;
}

// 0x4248f0
int DoorAt(int x, int y)
{
    for (int i = 0; i < 400; i++)
        if (gDoors[i].x == x && gDoors[i].y == y && gDoors[i].level == gCurLevel + 1) return i;
    return -1;
}

// 0x424940
int VisDoorAt(int x, int y)
{
    for (int i = 0; i < kMaxVisDoors; i++) {
        VisDoor &v = gVisDoors[i];
        if (!v.active) continue;
        if (gDoors[v.door].x == x && gDoors[v.door].y == y) return i;
    }
    return -1;
}

// 0x424990: 'S'earch.
void SearchSecrets()
{
    gUIMode = 7;
    gPlayer.noise += 0x14;
    if (!SearchAround()) ShowMessage(gMsg[19] /* You find nothing */, gColorWhite);
    gUIMode = 0;
}

// 0x4249e0: secret walls around the player.
int SearchAround()
{
    int found = 0;
    for (int k = 0; k < 8; k++) {
        int x = gDirDX[k] + gPlayer.tileX;
        int y = gDirDY[k] + gPlayer.tileY;
        int t = gLevelMap[gMapRow[y] + x];
        if (t != 0x1c && t != 0x1d) continue;
        if (AskOpenSecret() == 1) {
            SetMapCell(x, y, 2, 0xff);
            SetMapCell(x, y, 3, 0);
            g_5c5824 = 1;
            ShowMessage(gMsg[20] /* You have found a secret door!!! */, gColorWhite);
            RemoveWall(x, y);
            PlaySound(9, -1, -1);
            GainExperience(100);
            gPlayer.noise += 0x32;
        }
        found = 1;
    }
    return found;
}

// 0x424ac0
int AskOpenSecret()
{
    RedrawGameScreen(-1);
    Sprite *s = GrabScreen(0xf0, 0xc8, 0x190, 0x104);
    gText.SetColor(gColorWhite, 0);
    gText.PrintC(0xc8, (char *)gMsg[84] /* You have found a secret door */);
    gText.PrintC(0xd7, (char *)gMsg[85] /* Do you wish to open it ? */);
    AddDirtyRect(0, 0, 640, 480);
    int r = YesNoMenu();
    RestoreScreen(0xf0, 0xc8, s);
    gDDW.FillRect(0, 0, 0x27f, 0x1df, 0);
    return r;
}

// 0x424b80
int MouseInBox(int x, int y, int w, int h)
{
    return gMouseX >= x && gMouseX <= x + w && gMouseY >= y && gMouseY <= y + h;
}

// 0x424bc0
int MouseInRect(RECT *r)
{
    return gMouseX >= r->left && gMouseY >= r->top && gMouseX <= r->right && gMouseY <= r->bottom;
}

// 0x424c00: queue a message line (ignored when full or repeated).
void ShowMessage(const char *text, int color)
{
    int n = gNumMessages;
    if (n == 6) return;
    if (strlen(text) > 0x3b) return;
    if (n && !strcmp(gMessages[n - 1].text, text)) return;
    strcpy(gMessages[n].text, text);
    gMessages[n].color = (int16_t)color;
    gMessages[n].time = gTime + 200;
    gNumMessages = n + 1;
}

// 0x424cd0
void ExpireMessages()
{
    int n = gNumMessages;
    if (!n || gMessages[0].time >= gTime) return;
    for (int i = 0; i < n - 1; i++) gMessages[i] = gMessages[i + 1];
    gNumMessages = n - 1;
}

// 0x424d20
int DoorUnderMouse()
{
    for (int i = 0; i < kMaxVisDoors; i++) {
        VisDoor &v = gVisDoors[i];
        if (v.active == 1 && MouseInBox(v.sx, v.sy, 0x3e, 0x82)) return i;
    }
    return -1;
}

// 0x424d70
void EnterLevel(int level)
{
    fprintf(stderr, "Starting a new level : %d\n", level + 1);
    if (gLevelLoaded) return;
    SetCursor(6);
    ShowLoadingScreen();
    ClearFGList();
    gNumMessages = 0;
    SetupLevelMaps(level);
    LoadLevelMonsterModels(level);
    FindZoneRange();
    LoadPlayerModel();
    InitAnimTiles();
    FindFeatureRange();
    LoadFeatureSprites();
    FindObjRange();
    LoadObjSprites();
    FindItemRange();
    FindLightRange();
    FindPads();
    LoadDoorSprites();
    FindDoorRange();
    FixWallTiles();
    BuildBaseLight();
    BuildLightLists();
    UpdateLighting();
    SpawnRats(gLevelRatCount[level], gLevelRatPoints[level], &gLevelRatSpawns[level * 10]);
    ClearFloorCache();
    CompactInventory();
    StartEVSound();
    if (gPlayer.forceField) {
        SetSoundLoop(0x5a, 1);
        PlaySound(0x5a, -1, -1);
    }
    gLoopFrames = 0;
    gTarget = -1;
    if (level > gDeepestLevel) gDeepestLevel = level;
    SetCursor(0);
    gLevelLoaded = 1;
}

// 0x424ec0
void SetupLevelMaps(int level)
{
    gMapWidth = gLevelW[level];
    gMapHeight = gLevelH[level];
    LoadFloorTiles(gLevelName[level]);
    LoadWallSprites(gLevelTitle[level]);
    gLevelMap = gLevelLayout[level];
    gOverlayLayer = gLevelTiles[level];
    gWallLayer = gLevelWalls[level];
    gRegionLayer = gLevelAltWalls[level];
    for (int i = 0, r = 0; i < 65; i++, r += gMapWidth) gMapRow[i] = r;
}

// 0x424f60: pressure plates and trap doors of the level.
void FindPads()
{
    for (int i = 0; i < 20; i++) gPads[i][0] = -1;
    int n = 0;
    for (int x = 0; x < gMapWidth; x++) {
        for (int y = 0; y < gMapHeight; y++) {
            uint8_t t = gLevelMap[gMapRow[y] + x];
            if (t != 4 && t != 5) continue;
            gPads[n][0] = x;
            gPads[n][1] = y;
            if (++n == 0x14) fprintf(stderr, "Info: Too mant pads!!\n");
        }
    }
}

// 0x425000: load only the decoration sprites this level uses.
void LoadFeatureSprites()
{
    memset(gFeatureSpriteUsed, 0, sizeof(gFeatureSpriteUsed));
    if (gFeatFirst == -1 || gFeatLast == -1) return;
    for (int i = gFeatFirst; i <= gFeatLast; i++) {
        int s = gFeatures[i].sprite;
        gFeatureSpriteUsed[s] = 1;
        switch (gFeatureNameIdx[s]) {
        case 4: case 0x30: gFeatureSpriteUsed[s + 1] = 1; break;
        case 5: case 0x31: gFeatureSpriteUsed[s - 1] = 1; break;
        }
    }
    if (gCurLevel == 0xc) gFeatureSpriteUsed[0xc5] = 1;
    if (gCurLevel == 0x11) gFeatureSpriteUsed[0xf9] = 1;
    if (gCurLevel == 0x15) {
        gFeatureSpriteUsed[0x109] = 1;
        gFeatureSpriteUsed[0x10a] = 1;
    }
    gFeatureSpriteCount = LoadCSpriteTableA((char *)"levels\\decore.cst", gFeatureSprites, 400,
                                            (char *)gFeatureSpriteUsed);
}

// 0x425110
void LoadObjSprites()
{
    memset(gObjSpriteUsed, 0, sizeof(gObjSpriteUsed));
    if (gObjFirst == -1 || gObjLast == -1) return;
    for (int i = gObjFirst < 0 ? 0 : gObjFirst; i <= gObjLast; i++) {   // (-1: none)
        int k = gLevelObjs[i].kind;
        unsigned lo = gObjSpriteFirst[k], hi = gObjSpriteLast[k];
        if (lo <= hi) memset(&gObjSpriteUsed[lo], 1, hi - lo + 1);
    }
    gObjSpriteCount = LoadCSpriteTableA((char *)"levels\\andecore.cst", gObjSprites, 400, (char *)gObjSpriteUsed);
}

// 0x4251b0
void LoadDoorSprites()
{
    memset(gDoorSpriteUsed, 0, sizeof(gDoorSpriteUsed));
    for (int i = 0; i < 400; i++) {
        Door &d = gDoors[i];
        if (d.active == 1 && d.level == gCurLevel + 1) memset(&gDoorSpriteUsed[d.kind * 24], 1, 24);
    }
    gDoorSpriteCount = LoadCSpriteTableA((char *)"levels\\doors.CST", gDoorSprites, 0x28a, (char *)gDoorSpriteUsed);
}

// 0x425230
void LeaveGame()
{
    if (!gLevelLoaded) return;
    KillEVSound();
    StopAllSounds();
    ClearParticles();
    FreeLightLists();
    FreePlayerModel();
    FreeWallSprites();
    ClearDeadMonsters();
    ReleaseCSpriteTable(gDoorSprites, gDoorSpriteCount);
    ReleaseCSpriteTable(gFeatureSprites, gFeatureSpriteCount);
    gTimeStop = 0;
    gLocFxActive = 0;
    gLocFxKind = 0;
    gLocFxX = 0;
    gLocFxY = 0;
    gLocFxSprite = 0;
    gLocFxDepth = 0;
    gLocFxFrame = 0;
    gLocFxUnused = 0;
    memset(gProjectiles, 0, 0x8c * 4);
    memset(gBurners, 0, sizeof(gBurners));
    gLevelLoaded = 0;
}

// 0x4252b0: fatigue and death. Returns 0xd once the death animation is over.
int CheckPlayerDeath()
{
    if (gPlayer.gameMode == 0xc) {
        if (gPlayerAnim.AtEnd() && gPlayer.actionFrame == 0x1e) return 0xd;
        return -1;
    }
    gPlayer.fatigue += abs((int)(gPlayer.speed * 5.0f));
    if (gPlayer.fatigue > 250000 && (unsigned)gTicks % 0x444 == 0) {
        gStats.hp--;
        ShowMessage(gMsg[99] /* Starving!! */, gColorRed);
    }
    if (gStats.hp > 0) return -1;
    if (gEquip.ring1 == 3 || gEquip.ring2 == 3) {
        RingOfLife();
        return -1;
    }
    if (gPlayer.gameMode == 7)
        for (int s = 0x44; s < 0x49; s++) StopSound(s);
    gPlayer.magicShield = 0;
    gPlayer.gameMode = 0xc;
    gPlayer.actionFrame = 0;
    if (gPlayer.animKind) {
        gActionMode = 0;
        gEquip.shield = 0;
        gEquip.weaponType = 0;
        FreePlayerModel();
        LoadPlayerModel();
    }
    gPlayerModel = gPlayerAnim.Hold(rand() % 2 + 0xb);
    PlaySound(gStats.charClass == 3 ? 0x69 : 7, -1, -1);
    return -1;
}

// 0x425430: the animation set matching class, action and equipment.
void LoadPlayerModel()
{
    static const char *kShield[4] = {"NO", "SH", "TO", "TO"};
    static const char *kWeapon[21] = {"FI", "MA", "MA", "MA", "CR", "SW", "SW", "SW", "SW", "AX", "AX",
                                      "AX", "AX", "FI", "FI", "MA", "MA", "SW", "SW", "AX", "AX"};
    char letter = gClassLetters[gStats.charClass];
    char path[0x90];
    sprintf(gPlayerAmtName, "PL%cFI_NO.AMT", letter);
    gPlayer.animKind = 0;
    if (gPlayer.gameMode == 7) {
        sprintf(gPlayerAmtName, "PL%cFLUTE.AMT", letter);
        gPlayer.animKind = 3;
    } else if (gPlayer.gameMode == 5) {
        sprintf(gPlayerAmtName, "PL%cJUMP.AMT", letter);
        gPlayer.animKind = 2;
    } else if (gPlayer.gameMode == 6) {
        sprintf(gPlayerAmtName, "PL%cMAGIC.AMT", letter);
        gPlayer.animKind = 1;
    } else if (gActionMode == 0) {
        sprintf(gPlayerAmtName, "PL%c%s_%s.AMT", letter, kWeapon[gEquip.weaponType], kShield[gEquip.shield]);
        gPlayer.animKind = 0;
    }
    strcpy(path, "gamedat\\player\\");
    strcat(path, gPlayerAmtName);
    gPlayerAnim.Free();
    if (!gPlayerAnim.Load(path)) FatalError("Cannot load AMT file : %s", path);
    LoadPlayerTextures();
    gPlayerModel = gPlayerAnim.Loop(0);
}

// 0x425670
void FreePlayerModel()
{
    gPlayerAnim.Free();
    gPlayerTexture.Free();
}

static void ApplyPatch(const char *name, int y, const char *err)
{
    char path[0x100];
    Texture patch;
    patch.Construct();
    sprintf(path, "%s%c%s", gPlayerTexDir, gClassLetters[gStats.charClass], name);
    if (!patch.Load(path)) FatalError(err);
    gPlayerTexture.Blit(&patch, 0, y);
    patch.Free();
}

// 0x425690: the player's skin with armour, boots, weapon and shield painted on.
void LoadPlayerTextures()
{
    char path[0x100];
    int cls = gStats.charClass;
    gPlayerTexture.Free();
    sprintf(path, "%s%cmain.tex", gPlayerTexDir, gClassLetters[cls]);
    if (!gPlayerTexture.Load(path)) FatalError("Cannot load texture : %s", path);
    gPlayerAnim.SetTexture(&gPlayerTexture);
    int i = gArmourPatchIdx[gEquip.armourType];
    if (i != -1)
        ApplyPatch(gArmourPatchName[i], gArmourPatchY[gEquip.armourType + cls * 8], "Cannot load armour patch");
    i = gBootsPatchIdx[gEquip.e0c];
    if (i != -1) ApplyPatch(gBootsPatchName[i], gBootsPatchY[gEquip.e0c + cls * 4], "Cannot load boots patch");
    i = gWeaponPatchIdx[gEquip.weaponType];
    if (i != -1)
        ApplyPatch(gWeaponPatchName[i], gWeaponPatchY[gEquip.weaponType + cls * 21], "Cannot load weapon patch");
    i = gShieldPatchIdx[gEquip.shield];
    if (i != -1)
        ApplyPatch(gShieldPatchName[i], gShieldPatchY[gEquip.shield + cls * 4], "Cannot load left hand patch");
    if (gPlayer.stoneSkin) gPlayerTexture.ToGrey();
    DrawWounds();
}

// 0x425970
int LoadWallSprites(const char *name)
{
    char path[48];
    sprintf(path, "%s%s.CST", "levels\\", name);
    gWallSpriteCount = LoadCSpriteTable(path, gWallSprites, 200);
    return 1;
}

// 0x4259c0
void FreeWallSprites()
{
    for (int i = 0; i < gWallSpriteCount; i++) gWallSprites[i].Release();
}

// 0x4259f0: some weapons break now and then.
void MaybeBreakWeapon()
{
    if (!gWeaponBreaks[gEquip.weaponType]) return;
    if (rand() % 300) return;
    gEquip.weaponType = 0;
    ShowMessage(gMsg[202] /* Your weapon has broken.... */, gColorRed);
    LoadPlayerModel();
}

// 0x425a40: floor height (water, pits...) of a tile.
int TileHeight(int x, int y)
{
    switch (gLevelMap[gMapRow[y] + x]) {
    case 6: case 13: return 10;
    case 7: case 14: return 20;
    case 8: return 30;
    case 10: return 5;
    }
    return 0;
}

// 0x425ab0: redraw the game view (optionally tinted) behind a dialog.
void RedrawGameScreen(int tint)
{
    static const uint8_t kR[5] = {0x80, 0x80, 0, 0x80, 0};
    static const uint8_t kG[5] = {0, 0, 0, 0x80, 0x80};
    static const uint8_t kB[5] = {0x80, 0, 0x80, 0x80, 0};
    int cur = GetCursor();
    SetCursor(6);
    gDDW.FillRect(0, 0, 0x27f, 0x1df, 0);
    ComputeView(gPlayer.tileX, gPlayer.tileY, 0);
    RenderView();
    DrawPlayer();
    DrawLevelObjs();
    DrawMonsters();
    DrawFGObjects();
    DrawRats();
    DrawHud();
    if (tint >= 0) gDDW.TintRect(0, 0, 0x27f, 0x1df, gDDW.MakePixel16(kR[tint], kG[tint], kB[tint]));
    SetCursor(cur);
}

// 0x425bb0: scripted story events. Returns 7 when the game is won.
int CheckStoryEvents()
{
    int tx = (int)gPlayer.x, ty = (int)gPlayer.y;
    if (gCurLevel == 1 && !g_5a6590 && gLoopFrames == 4) {
        RunConversation(&gConvHeaders[0]);
        g_5a6590 = 1;
    }
    if (gCurLevel == 4 && ty >= 0x12 && ty <= 0x14 && !gMetJetraal && GetAttitudeOfType(0x27) == 1 &&
        tx >= 0x13 && tx <= 0x15) {
        RunConversation(&gConvHeaders[1]);
        gMetJetraal = 1;
        KillMonster(4, 0);
    }
    if (gCurLevel == 0xc && tx == 7 && ty == 6 && !g_5a6593) {
        RunConversation(&gConvHeaders[6]);
        g_5a6593 = 1;
    }
    if (gCurLevel == 0x18) {
        if (gStatuesDestroyed == 5 && !gTalkedToPriest) PriestOpensGate();
        if (gCurLevel == 0x18 && tx == 9 && ty == 0x18) return 7;
    }
    return 0;
}

// 0x425d10: the nearest decoration under the mouse.
int FeatureUnderMouse()
{
    int best = -1;
    float bestDepth = 3000.0f;
    for (int i = 0; i < kMaxVisFeatures; i++) {
        VisFeature &v = gVisFeatures[i];
        if (v.active != 1 || !MouseInRect((RECT *)v.rect)) continue;
        if (v.fg.depth < bestDepth) {
            best = i;
            bestDepth = v.fg.depth;
        }
    }
    return best;
}

// 0x425d70
int ObjectUnderMouse()
{
    for (int i = 0; i < 200; i++) {
        LevelObj &o = gLevelObjs[i];
        if (o.active != 1 || o.level != gCurLevel) continue;
        if (gMapVis[o.x * 65 + o.y] >= 0xfe) continue;
        if (MouseInRect((RECT *)&o.x0)) return i;
    }
    return -1;
}
