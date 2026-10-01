// Prototypes of the decompiled game functions (original address in comments).
#pragma once
#include <stdint.h>

// conv.cpp
void PlayFallingMovie();                                   // 0x401200
void PlaySmashMovie();                                     // 0x4012f0
void UpdateCDMusic();                                      // 0x4013f0
void PlayCDTrack(int track);                               // 0x401470
void StopCDMusic();                                        // 0x401480
void CDVolumeDialog();                                     // 0x401490
void ResetConversations();                                 // 0x4014f0
void LinkConversations();                                  // 0x401510
void ConvAddNode(ConvHeader *h, ConvNode *n);              // 0x401710
int RunConversation(ConvHeader *h);                        // 0x401760
void ConvPrintText(ConvHeader *h, const char **table, int index); // 0x401920
ConvNode *ConvChooseTopic(ConvNode *list);                 // 0x4019d0
int ConvSelectTopic(ConvHeader *h, ConvNode *n);           // 0x401b80
void ConvPrintLine(ConvHeader *h, int y, const char *s);   // 0x401c20
void ConvShowTopic(ConvNode *n, int topic);                // 0x401de0
void ConvHideTopic(ConvNode *n, int topic);                // 0x401e10
int ConvPayForAnswer(ConvHeader *h, int text, int cost);   // 0x401e40
void PlayConvSpeech(ConvHeader *h, int kind, int index);   // 0x401fe0

// effects.cpp
void ClearWounds();                                        // 0x402140
void AddWound(int lower, int right);                       // 0x402160
void DrawWounds();                                         // 0x402260
void HealWounds(int keep);                                 // 0x4022c0
int CountWounds();                                         // 0x402300
void StaticInit_HotkeyMenu();                              // 0x402370
void ClearSpellHotkeys();                                  // 0x4026e0
void HotkeyDialog();                                       // 0x402700
void BuildHotkeyMenu();                                    // 0x4027f0
void HotkeySelectSpell(int slot);                          // 0x402940
int BuildSpellMenu(int first, int last, MenuItem *items);  // 0x402bc0
void UseSpellHotkey(int key);                              // 0x402c70
void SparkBurst(float x, float y);                         // 0x402d30
void LightningAroundPlayer();                              // 0x402e30
void LightningArc(float x, float y);                       // 0x402f30
void GroundSpark(float x, float y);                        // 0x403060
void BoltBetween(Vec3 *from, Vec3 *to);                    // 0x403150
void SparkDirected(float x, float y, float z, int angle);  // 0x4031b0
void UpdateBolts();                                        // 0x403290
void DrawBolts();                                          // 0x4032c0
int FindFreeBolt();                                        // 0x4032f0

// monster.cpp
void StaticInit_Monsters();                                // 0x403690
void StaticInit_MonsterAnims();                            // 0x4036d0
void StaticInit_MonsterTextures();                         // 0x403700
void InitAllMonsters();                                    // 0x403720
void KillMonster(int level, int idx);                      // 0x403760
void RelinkMonsters();                                     // 0x403790
void LoadLevelMonsterModels(int level);                    // 0x4037e0
int MonsterUnderMouse();                                   // 0x403820
void UpdateMonsters();                                     // 0x4038c0
Monster *GetMonster(int level, int idx);                   // 0x403910
void TalkToMonster(int idx);                               // 0x403940
int ProjectileHitsMonster(Projectile *p);                  // 0x403ab0
void MonstersCheckTraps();                                 // 0x403b10
void DrawMonsters();                                       // 0x403b40
int CountMonsters(char *flags, int attitude);              // 0x403b70
int FindFightTarget();                                     // 0x403c30
void PlayerStrike();                                       // 0x403d40
int StrikeAt(float x, float y, int angle);                 // 0x403dd0
void SummonServant(int x, int y);                          // 0x403e70
void PlaceLevel24Guards();                                 // 0x403f10
void PriestOpensGate();                                    // 0x403fa0
void SpawnDemonShade(float x, float y, int angle);         // 0x404020
void ExpireEnchantments();                                 // 0x404080
void UpdateMonsterVisibility();                            // 0x404140
int LoadMonsterModel(MonsterStats *st, AnimSet *dst);      // 0x4041a0
int MonsterSpellKill(int idx);                             // 0x404320
void DamageAllMonsters(int dmg);                           // 0x404360
int MonsterSpellWeaken(int idx);                           // 0x4043e0
void FearMonster(int idx, unsigned level, int time);       // 0x404420
void HoldMonster(int idx, unsigned level, int time, int verbose); // 0x404500
int MonsterIsActive(int level, int idx);                   // 0x4045d0
void SetAttitudeOfType(int type, int attitude);            // 0x404600
int GetAttitudeOfType(int type);                           // 0x404650
int AngleToMonster(int idx);                               // 0x4046a0
void RegenerateMonsters();                                 // 0x4046f0
void ClearDeadMonsters();                                  // 0x404760
MonsterCell *MonsterCellAt(int x, int y);
void ClearMonsterMap();                                    // 0x408130
int MarkMonster(int x, int y, int id);                     // 0x408160
void UnmarkMonster(int x, int y, int id);                  // 0x4081c0
int MonsterCellOccupied(int x, int y);                     // 0x408220
uint8_t MonsterAtPoint(float x, float y, float r);         // 0x408250

// objects.cpp
int PlanPath(float x, float y, int angle, float tx, float ty); // 0x408310
int FollowPath(Player *pl);                                // 0x408c50
void LoadProjectileModels();                               // 0x408d60
void FreeProjectileModels();                               // 0x408ea0
int FireProjectile(float x, float y, int angle, int kind, int flags); // 0x408ef0
void UpdateProjectiles();                                  // 0x408fd0
void ProjectileFly(Projectile *p, int index);              // 0x409030
int ProjectileHitsPlayer(Projectile *p);                   // 0x409100
void ProjectileExploding(Projectile *p, int index);        // 0x409190
int ProjectileExplode(Projectile *p);                      // 0x4091c0
int TileIsOpen(int x, int y);                              // 0x409280
void DropArrow(int x, int y);                              // 0x4092a0
void DrawProjectiles();                                    // 0x409390
void DrawProjectileModel(Projectile *p, int light, bool tileLit = false);        // 0x409440
int ProjectileDamage(int kind);                            // 0x409520
void PressurePlate(int x, int y, int showMsg);             // 0x4095e0
void StaticInit_RatModel();                                // 0x4096f0
void StaticInit_RatTex();                                  // 0x409710
void LoadRatModel();                                       // 0x409720
void FreeRatModel();                                       // 0x409770
void SpawnRats(int count, int numPoints, int *points);     // 0x409790
void UpdateRats();                                         // 0x4097e0
void DrawRats();                                           // 0x409810
void RatInit(Rat *r, int *pt);                             // 0x409840
void RatUpdate(Rat *r);                                    // 0x409900
void RatDraw(Rat *r);                                      // 0x409b60
void StaticInit_ServantAnim();                             // 0x409c60
void StaticInit_ServantTex();                              // 0x409c80
void LoadServantModels();                                  // 0x409c90
void RelinkServants();                                     // 0x409da0
void FreeServantModels();                                  // 0x409dd0
void ClearServants();                                      // 0x409e10
int SpawnServant(float x, float y);                        // 0x409e30
void UpdateServants();                                     // 0x409f10
void DrawServants();                                       // 0x409f40
void ServantUpdate(Servant *s);                            // 0x409f70
void ServantSeek(Servant *s);                              // 0x40a000
void ServantFight(Servant *s);                             // 0x40a160
void ServantWalk(Servant *s);                              // 0x40a280
void ServantAttack(Servant *s);                            // 0x40a2e0
int ServantAngleTo(Servant *s, float x, float y);          // 0x40a350
int ServantFindTarget();                                   // 0x40a370
int ServantsTargeting(int idx, int limit);                 // 0x40a490
int ServantInReach(Servant *s);                            // 0x40a4d0
void ServantSetDest(Servant *s, float tx, float ty);       // 0x40a520
void ServantHitTarget(Servant *s);                         // 0x40a650
void DismissServants(int keep);                            // 0x40a690
int CountServants();                                       // 0x40a6e0
void ShiftServants(int oldX, int oldY, int newX, int newY); // 0x40a700
void ServantDraw(Servant *s);                              // 0x40a760
void ServantVanish(Servant *s);                            // 0x40a8d0
void AddDirtyRect(int x, int y, int w, int h);             // 0x40a930
void RestoreDirtyRects();                                  // 0x40a970
void UpdateAndRestore(DirectDrawWindow *d);                // 0x40a9c0
void FreeDirtyRects();                                     // 0x40a9d0

// model.cpp: AnimSet, Model, Texture methods (see types.h)

// mouse.cpp
void LogPrintf(Log *log, const char *fmt, ...);            // 0x40bce0
void StaticInit_Cursors();                                 // 0x40be00
int MouseInit(DirectDrawWindow *ddw, void *hwnd);          // 0x40be20
int MouseStartTimer();                                     // 0x40bfa0
void MousePreFlip();                                       // 0x40bff0
void MousePostFlip();                                      // 0x40c070
void MouseStopTimer();                                     // 0x40c0e0
void MouseShutdown();                                      // 0x40c100
void MouseAcquire(int on);                                 // 0x40c2d0
int MouseRead(int *px, int *py);                           // 0x40c300
void MouseSetPosition(int x, int y);
void MouseUpdateRange();
void SetScreenOriginY(int oy);
void MouseDrawCursor(int page, uint8_t *dst, unsigned long pitch); // 0x40c430
void MouseSaveUnder(int page, uint8_t *src, unsigned long pitch);  // 0x40c500
void MouseRestoreUnder(int page, uint8_t *dst, unsigned long pitch); // 0x40c660
void SetMouseSpeed(int n);                                 // 0x40c720
int LoadCursors(char *file);                               // 0x40c730
void SetCursor(int i);                                     // 0x40c770
int GetCursor();                                           // 0x40c7a0
void ShowMouse(int show);                                  // 0x40c7b0
void ResetMouseClicks();                                   // 0x40c840
int MouseRightClicked();                                   // 0x40c860
int MouseLeftClicked();                                    // 0x40c880
int MouseButtonDown(int button);                           // 0x40c8a0

// system.cpp
int PlayIntroMovie();                                      // 0x40d000
int MovieInterrupt();                                      // 0x40d0e0
int FindCD();                                              // 0x40d130
void PlayIntro2Movie();                                    // 0x40d1e0
void KeyPush(int ch, unsigned vk);                         // 0x40d300
int KeyPop(uint8_t *out);                                  // 0x40d370
void KeyClear();                                           // 0x40d3c0
bool KeyTakeEsc();                                         // port
bool KeyPeek(uint8_t *out);                                // port
void PlayerStop();                                         // port
void LoadLevelHeaders(const char *dir);                    // 0x40d3e0
void ClearLevelObjects();                                  // 0x40d530
void AllocLevel(int lvl, int w, int h);                    // 0x40d5a0
void FreeLevels();                                         // 0x40d640
void LoadAllLevels(const char *dir);                       // 0x40d6c0
int LoadLevel(const char *path, int lvl);                  // 0x40d700
void LoadLevelDoors(int fd);                               // 0x40d920
void LoadLevelItems(int fd);                               // 0x40d9c0
void LoadLevelFeatures(int fd);                            // 0x40da60
void LoadLevelLights(int fd);                              // 0x40daf0
void LoadLevelAux(int fd);                                 // 0x40db80
int GameMain();                                            // 0x40dc00 (WinMain)
long WndProc(void *hwnd, unsigned msg, unsigned long wParam, long lParam); // 0x40dce0
void PumpMessages();                                       // 0x40dfb0
int RunMenu(MenuItem *items, int count, int hiColor, void (*idle)(int, int, int), int flash); // 0x40e4e0
void DrawMenu(int sel, int hiColor);                       // 0x40e880
void DrawMenuItem(int x, int y, const char *text, int color, int hiColor); // 0x40e8f0
int HasMMX();                                              // 0x40e950

// math.cpp
void Normalize(Vec3 *v);                                   // 0x40e030
void TriangleNormal(Vec3 *out, Vec3 *p0, Vec3 *p1, Vec3 *p2); // 0x40e0a0
void InitTrigTables();                                     // 0x40e170
float CosF(float deg);                                     // 0x40e1d0
float SinF(float deg);                                     // 0x40e230
float CosDeg(int deg);                                     // 0x40e290
float SinDeg(int deg);                                     // 0x40e2e0
void RotateY(Vec3 *in, Vec3 *out, float deg);              // 0x40e330
void IsoTilt(Vec3 *in, Vec3 *out);                         // 0x40e3b0
int AngleBetween(float x0, float y0, float x1, float y1);  // 0x40e400

// render3d.cpp (Model methods in types.h)
void StaticInit_CurFrame();                                // 0x40f5e0
void SetModelCutY(float y);                                // 0x40f5f0
void SetRenderShade(Shade16 *s);                           // 0x40f600
void SetModelOpaque(int on);                               // 0x40f610
void GetModelRect(RECT *r);                                // 0x40f620
void SetModelVertexScale(float s);                         // 0x40f650

// particles.cpp (Particle methods in types.h)
void StaticInit_Particles();                               // 0x411880
int FindFreeParticle();                                    // 0x4118a0
void UpdateParticles();                                    // 0x4118e0
void ClearParticles();                                     // 0x411900
void DrawParticles();                                      // 0x411920
void BloodSplat(float x, float y, int angle, int type);    // 0x411980
void FireTrail(float x, float y);                          // 0x411ad0
void Smoke(float x, float y, int count);                   // 0x411c10
void Splash(float x, float y, int type);                   // 0x411d00
void FireBreath(float x, float y, int angle);              // 0x411ee0
void DrawSpit(float x, float y, int angle);                // 0x412050
void GroundGlow(int x, int y);                             // 0x4121b0
void Sparks(float x, float y, int angle);                  // 0x4122e0
void DrawDot(int x, int y, int n, uint8_t *surf, int pitch, uint16_t c, uint8_t shift); // 0x4126d0

// sound.cpp
void SoundInit();                                          // 0x412790
void SoundShutdown();                                      // 0x412810
void PreloadSounds();                                      // 0x412830
int LoadSound(int i);                                      // 0x412870
void FreeAllSounds();                                      // 0x412950
void FreeSound(int i);                                     // 0x412970
int PlaySound(int id, int x, int y);                       // 0x4129a0
void EvictOldSound();                                      // 0x412ae0
void SetSoundLoop(int id, int loop);                       // 0x412b40
void StopSound(int id);                                    // 0x412b60
void StopAllSounds();                                      // 0x412b80
void PlayFootstep();                                       // 0x412bc0
void WaitSound(int id);                                    // 0x412cc0
int SoundFinished(int id);                                 // 0x412d10
int PlaySpeechFile(const char *name);                      // 0x412d40
extern bool (*gSpeechSkipCheck)();                         // port
extern bool gSpeechSkipped;                                // port
void StartEVSound();                                       // 0x412dd0
void KillEVSound();                                        // 0x412ea0
void UpdateEVSound(int i);                                 // 0x412f80
void SoundOptionsMenu();                                   // 0x413080
void WavVolumeDialog();                                    // 0x4132c0
int GetWaveVolume();                                       // 0x413300
void SetWaveVolume(int n);                                 // 0x413340
void StartClock();                                         // 0x413380
void StopClock();                                          // 0x4133f0
void WaitTicks(int n);                                     // 0x413460
int CDAudio_Open(CDAudio *cd);                             // 0x413480
void CDAudio_SetTimeFormat(CDAudio *cd, int fmt);          // 0x4134f0
int CDAudio_Play(CDAudio *cd, int track);                  // 0x413520
void CDAudio_Stop(CDAudio *cd);                            // 0x4135a0
int CDAudio_Close(CDAudio *cd);                            // 0x4135d0
int CDAudio_GetPosition(CDAudio *cd);                      // 0x413610
int CDAudio_Poll(CDAudio *cd);                             // 0x413660
void CDAudio_SetVolume(CDAudio *cd, int vol);              // 0x413690
int CDAudio_GetVolume(CDAudio *cd);                        // 0x413710
int TitleMenu();                                           // 0x4137a0
int TitleMenuChoice();                                     // 0x4137f0

// chargen.cpp
int PlayIntro();                                           // 0x4139c0
void DrawTitleScreen();                                    // 0x413a10
void ShowLoadingScreen();                                  // 0x413b30
void StaticInit_CGModels();                                // 0x413c90
void StaticInit_CGTex();                                   // 0x413cc0
int CharacterGeneration();                                 // 0x413ce0
void DrawChargen(int flags);                               // 0x414180
int ChooseClass();                                         // 0x414270
void ChargenIdle(int sel, int, int);                       // 0x414570
int EnterName();                                           // 0x414640
int TextInput(char *buf);                                  // 0x4146e0
int RollStats();                                           // 0x414890
void RollCharacter();                                      // 0x414ad0
void DrawStats(int x, int y);                              // 0x414b60
int Roll4to7();                                            // 0x414c90
void ClassBonus();                                         // 0x414cb0
void FatalError(const char *fmt, ...);                     // 0x414d50


// ---- hud.cpp (0x414e20-0x415640) ----
void DrawHud();
void DrawHudPanel(int flags);
void DrawActionIcon(int big);
void DrawSpellIcon();
uint8_t MouseRepeat(int *counter);
void DrawStatusBar(int v);
void TileToScreen(int tx, int ty, int *sx, int *sy);
void WorldToScreen(float x, float y, int *sx, int *sy);
void ScreenToWorld(int sx, int sy, float *wx, float *wy);
Sprite *GrabScreen(int x, int y, int w, int h);
void RestoreScreen(int x, int y, Sprite *s);

// ---- scene.cpp (0x415640-0x417700) ----
void FindFeatureRange();
void FindItemRange();
void FindObjRange();
void FindLightRange();
void FindDoorRange();
void ResetScene();
void ComputeView(int x, int y, int secret);
void RenderView();
void FloodView(int x, int y, int secret);
int AddTile(int x, int y, int sx, int sy, int tile);
void AddWallSprite(int x, int y);
void ComputeLighting();
void CollectItems();
void DrawItems();
void DrawDecals();
void CollectFeatures();
void AddFeature(int i);
void CollectDoors();
void AddDoor(int i);
void DrawLevelObjs();
void InitAnimTiles();
void UpdateAnimTiles();
void DrawAnimTile(AnimTile *t);
void DrawFloorTiles();
void MarkAutomap(int x, int y);
void DrawWalls();
void DrawDoors();
void DrawFeatures();


// ---- fglist.cpp (0x417700-0x419010) ----
void DrawCharacterSheet();
int RectsOverlap(const RECT *a, const RECT *b);
void ResetFGList();
void ClearFGList();
void AddFGObject(FGObject *o);
void DrawFGObjects();
void DrawFGShadow(FGObject *o);
void DrawModelShadow(float x, float y, Model *m, int frame, int angle, int flag);
void CastShadow(FGObject *o, float lx, float ly, int height, int range);
float ScreenDepth(float x, float y);
void DrawLine(int x0, int y0, int x1, int y1, int color, int blend);
void DrawLineH(int x0, int y0, int x1, int y1, int color, int blend);
void DrawLineV(int x0, int y0, int x1, int y1, int color, int blend);
void DrawEffectIcons();
void DrawContainer(int idx);
void DrawMessages();
void DrawEquipment();
void AddPlayerLight();
int TileLightLevel(int x, int y);
int TileDistance(int x0, int y0, int x1, int y1);
float DistanceB(float x0, float y0, float x1, float y1);
float Distance(float x0, float y0, float x1, float y1);
int LightFalloff(int16_t level, float range, float dist);
void RemoveWall(int x, int y);
void RevealRegion(int x, int y);
void DrawShopGrid(int sel);
void ClearWallFlags();
void SetMapCell(int x, int y, int layer, uint8_t v);
float CarriedWeight();                                      // 0x420a00
void RecalcStats();                                         // 0x4282d0


// ---- lighting.cpp (0x419010-0x41a8e0) ----
void BuildBaseLight();
void BuildLightLists();
void FreeLightLists();
void UpdateLighting();
void FindZoneRange();
int LineOfSight(float x0, float y0, float x1, float y1, int flag);
int LineOfSightX(float x0, float y0, float x1, float y1);
int LineOfSightY(float x0, float y0, float x1, float y1);
void FixWallTiles();
uint8_t TileLight(int x, int y);
int LightInView(const int32_t *r);
void DetectTrap(int x, int y, int longer);
void DrawSparkles();
void StaticInit_CastMenu();
void StaticInit_SpellLevelMenu();
void StaticInit_ActionMenu();
void DrawRunes(int y0, int y1);


// ---- spells.cpp (0x41a8e0-0x41b310) ----
int CanMemorize(int spell);
void MemorizeDialog();
int BuildMemorizeMenu(int first, int last, MenuItem *menu);
void CastDialog();
int BuildCastMenu(int first, int last, MenuItem *menu);


// ---- spellcast.cpp (0x41b310-0x41d180) ----
void BeginCast(int spell);
int CompleteCast(int spell);
void MissileVolley(int kind);
void HealSpell(float amount, int lo, int hi);
void SlowSpell();
void FearSpell();
void TimeStopSpell();
void KnockSpell();
void RevealSpell();
void RevealItems();
void RevealSecretDoors();
void RevealTraps();
void RemoveCurse();
int SetEffectTimer(int base, int perLevel);
int PickTargetScreen();
void DrawForceField();
void AddOrbiter(int angle, int height, float *outX, float *outY);
void StartLocationEffect(float x, float y, int kind);
void UpdateLocationEffect();
void LightSpell(int kind);
void DrawLightSpellAt(int sx, int sy);
void DrawLightSpell();
void RingEffects();
void CureDisease();
int FindFreeDirection();
void TeleportSpell();
void CancelSpellDialog();
int ConfirmDialog();


// ---- floor.cpp (0x41d5c0-0x41ddb0) ----
void InitFloorCache();
int LoadFloorTiles(const char *name);
void DecodeFloorTile(CSprite16 *spr, uint16_t *out);
void FreeFloorTiles();
uint16_t *GetFloorSprite(int tile, uint16_t *lights);
void UpdateFloorCache();
void ClearFloorCache();
void ShadeTileGouraud(uint16_t *out, const uint16_t *src, const uint16_t *l);
void ShadeTileFlat(uint16_t *out, const uint16_t *src);
void FreeFloorCache();
void DrawFloorFast(uint16_t *pix, int x, int y);
void DrawFloorClipped(uint16_t *pix, int x, int y);


// ---- gameloop.cpp (0x41e480-0x41f9f0) ----
int GameInit();
void GameRun();
void GameShutdown();
void OnWindowMove();
void OnActivateApp(int active);
void PollMouse();
void MainMenuLoop();
void LoadPrefs();
void SavePrefs();
void LoadStaticGfx();
void FreeStaticGfx();
void PlayGame();
void ResetWorld();
void NewGame();
int GameLoop();
void UpdateTickRate();
void UpdateCursor();
int ReadKey();
int ProcessKeys();

// ---- staticinit.cpp ----
void StaticInit_PlayerAnim();
void StaticInit_PlayerTexture();
void StaticInit_ProjectileModels();
void StaticInit_ProjectileTextures();
void StaticInit_CD();
void StaticInit_SummonModels();
void StaticInit_SummonTextures();
void StaticInit_ServantAnims();
void StaticInit_Log();
void StaticInit_SaveMenu();                              // 0x42a7c0
void RunStaticInitializers();


// ---- ui.cpp (0x41f9f0-0x420800) ----
int ProcessMouse();
void GameMouse();
void DropModeMouse();
void UseModeMouse();
void InfoModeMouse();
void LeaveInfoMode();
int ItemsUnderMouseInfo();
void DrawInfoText(int);
int MouseInHud();
int HudClick();
int HudButtonAt();
int InventoryArrows();
void CycleActionMode();
void ScrollInventory(int dir);
void DropInventoryItem(int slot);
int CanDropAt(int x, int y);
void SortLevelItems();


// ---- items.cpp (0x420800-0x421f10) ----
int PickUpItems();
int DropItem(int x, int y, int type, int qty);
int CollectPickables();
int ChoosePickMenu(int *list, int n);
int IsPickable(int idx);
void ShowPickupMessage(int type);
void CompactInventory();
int FindFreeInvSlot();
int OptionsMenu();
void ShadowOptionsMenu();
int QuitConfirm();
int YesNoMenu();


// ---- player.cpp (0x421f10-0x4244f0) ----
void UpdatePlayer();
void PlayerIdle();
void MeleeAttack();
void ShootBow();
int SetWalkTarget();
void PlayerWalk();
void PlayerBackstep();
void PlayerAttack();
void PlayerCastWindup();
void PlayerCast();
void PlayerShoot();
void PlayerJumpCharge();
void PlayerJump();
void PlayerPlayMusic();
void PlayerHurt();
void PlayerTeleport();
void PlayerTurnRight();
void PlayerTurnLeft();
void StartTurn();
int CanWalk();
void DoPendingAction();
int CanMoveTo(Player *p, float dist);
void DamagePlayer(int dmg, int bleed);
void PlayerKnockback(int behind);
int CircleOverlap(float x0, float y0, float r0, float x1, float y1, float r1);
void ScrollCamera(float x, float y);
void PlayerSwitchWeapon();
void CheckPlayerTile();
void TakeStairs(int x, int y);
void DrawPlayer();
void DrawPlayerModel(int);
void ShieldJitter();


// ---- world.cpp (0x4244f0-0x425d10) ----
void UpdateDoors();
void OpenNearbyDoors();
void UseDoor(int vi);
int DoorReachable(int di);
int DoorAt(int x, int y);
int VisDoorAt(int x, int y);
void SearchSecrets();
int SearchAround();
int AskOpenSecret();
int MouseInBox(int x, int y, int w, int h);
int MouseInRect(RECT *r);
void ShowMessage(const char *text, int color);
void ExpireMessages();
int DoorUnderMouse();
void EnterLevel(int level);
void SetupLevelMaps(int level);
void FindPads();
void LoadFeatureSprites();
void LoadObjSprites();
void LoadDoorSprites();
void LeaveGame();
int CheckPlayerDeath();
void LoadPlayerModel();
void FreePlayerModel();
void LoadPlayerTextures();
int LoadWallSprites(const char *name);
void FreeWallSprites();
void MaybeBreakWeapon();
int TileHeight(int x, int y);
void RedrawGameScreen(int tint);
int CheckStoryEvents();
int FeatureUnderMouse();
int ObjectUnderMouse();


// ---- features.cpp (0x425dd0-0x427630) ----
void UseFeature(int vi);
int FeatureClickable(int vi);
void UseObject(int i);
void UseStairsDown(int vi);
void UseStairsUp(int vi);
void ChangeLevelAt(int x, int y);
int FeatureInReach(int vi);
void ReadSign(int vi);
void FallStairsDown(int vi);
void FallStairsUp(int vi);
void UpdatePoisonDisplay(int force);
void DrawHpBar();
void DrawManaBar();
void ShowHealth();
void ShowMana();
void ShowFatigue();
void GainExperience(int xp);
void GainMagicExperience(int xp);
void CheckStandingTile();
void OpenChest(int vi);
void DrawChestContents(int ci);
int ChestUnlock(Container *c);
void ChestLockUI(Container *c);
void ChestLootUI(int ci);
int ChestSlotAtMouse();
void PutInChest(int ci, int slot);
int FreeChestSlot(int ci);
void TakeFromChest(int ci, int cs);

// ---- mechanisms.cpp (0x427630-0x429550) ----
void ReadBookshelf(int vi);
void ReadBook(const char *path, int pcxIdx);
void CheckWarpTile();
void Warp(int x, int y, int kind);
int FindWarp(int x, int y);
void BreakWall();
void RemoveFeature(int level, int x, int y);
void AddFeatureAt(int level, int x, int y, int sprite);
void SetFeatureSprite(int x, int y, int sprite);
int FireArrow();
int AimAngle();
int Dice(int base, int lo, int hi, int n);
void UpdateLightRadius();
void PullLeverA(int vi);
void PullLeverB(int vi);
void CheckPads();
void PressPlate(int x, int y);
void ReleasePlate(int x, int y);
void SetTrigger(int x, int y, int state);
void TriggerWalls(int param, int ok);
void TriggerDoor(int param, int ok);
void TriggerFeatures(int param, int ok);
void TriggerWarp(int param, int ok);
void TriggerRemoveFeatures(int param, int ok);
void FlashScreen(int color);
void UseMap();
void DrawHints();
void TalkToMushroom();

// ---- invscreen.cpp (0x429550-0x42a7c0) ----
void MouseSpeedDialog();
void GammaDialog();
void InventoryScreen();
void DrawInventoryScreen();
void InventoryLoop();
int EquipSlotAtMouse();
void EquipItem(int inv);
void UnequipItem(int slot);
void UpdateWalkSpeed();
void StatusText(const char *text);
void SetStatusY(int y);
int DrawStatusText();
void ClearStatusText();
int InventorySlotAtMouse();
void UseInventoryItem(int slot);
void UseMiscItem(int item, int slot);
void DrinkAntidote(int slot);
void UseKey(int item, int slot);
void UseBandages(int slot);
void UseElixir();
void DrinkElixir(int slot);
void PickLock(int slot);
void RemoveInventoryItem(int slot);

// ---- savegame.cpp (0x42ab40-0x42b920) ----
void SaveGameDialog();
void ReadSaveHeaders();
int SaveSlotMenu();
int EditSaveName(char *s);
void SaveGame(int slot);
int LoadGameDialog();
int SaveExists(int slot);
void LoadGame(int slot);

// ---- timers.cpp (0x42b920-0x42cf90) ----
void UpdateTimers();
int FindInventoryItem(int item);
void Rest();
int RestDialog();
void SleepHours(int hours);
int VolumeDialog(int min, int max, char *pcxName, const char *title, int cur);
void EatNightshade(int slot);
void EatNightshadeStalk(int slot);
void EatFood(int slot);
int CheckPit();
void AddAnimTile(int x, int y, int kind);
void RingOfLife();
void UseOrb(int slot);
void PlayFlute();
void DrinkFountain(int obj);
void TrapGoesOff(int x, int y);
void UpdateTrap();
void UseDemonCrusher();
int CountItemsAt(int x, int y);
int ItemTypeAt(int x, int y);
void RemoveItemsOfType(int type);
void ReadDemonLore();
void AddBurner(int x, int y);
void UpdateBurners();

// ---- shop.cpp (0x42cf90-0x42d9b0) ----
void RingBell(int vi);
void ShopScreen();
int BuyItem(int i);
int ShopSlotAtMouse();
void OfferToStatue(int vi);
int PickInventoryItem();
int IsTreasure(int item);
void ReadScroll(int slot);
void BloodPool(int x, int y, int type);


// ---- capacities of the per-frame view lists (enlarged over the original
// 550 tiles, 200 walls, 300 items, 200 features, 12 doors and 350
// foreground objects so the widescreen view fits)
template <typename T, size_t N> constexpr int CountOf(T (&)[N]) { return (int)N; }
#define kMaxTiles CountOf(gTiles)
#define kMaxWalls CountOf(gWalls)
#define kMaxVisItems CountOf(gVisItems)
#define kMaxVisFeatures CountOf(gVisFeatures)
#define kMaxVisDoors CountOf(gVisDoors)
#define kMaxFGObjects CountOf(gFGPool)

// ---- screen extents of the map view: the original 640 pixels, or the whole
// widescreen framebuffer in the enhanced mode (bottom edge unchanged)
static inline int ViewL() { return gLayout.wide() ? gLayout.minX() : 0; }
static inline int ViewT() { return gLayout.wide() ? gLayout.minY() : 0; }
static inline int ViewR() { return gLayout.wide() ? gLayout.maxX() : 0x27f; }
// The row models, shadows, lines and particles stop at: 400 in the original
// (also with the status bar hidden); the enhanced mode uses the view bottom.
inline int ViewCap() { return gLayout.wide() ? gViewHeight + 1 : 400; }

// ---- port: coloured lighting (enhanced mode, after the 3dfx build rpg3dfx.exe)
extern bool gColorLight;
extern uint16_t gBaseRGB[4550], gDynRGB[4550], gLightRGB[4550];
uint16_t AddLightColor(uint16_t c, float level, float r, float g, float b);
uint16_t AverageLightColor(uint16_t a, uint16_t b, uint16_t c, uint16_t d);
uint16_t TileLightRGB(int x, int y);
void SetTileShade(int idx);
void SetModelLightRGB(uint16_t c);
uint16_t *GetFloorSpriteRGB(int tile, const uint16_t *colors);
void DrawLineAA(int x0, int y0, int x1, int y1, int color, int blend);
void SetModelFilter(bool on);
#include "settings.h"
