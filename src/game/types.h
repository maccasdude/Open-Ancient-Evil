// Hand-written game types (classes with behaviour; plain data structures are
// generated from re/structs.def into globals.h).
#pragma once
#include <stdint.h>
#include <stdio.h>
#include "../engine/ddw16.h"
#include "../platform/media.h"
#include "../platform/snd.h"

struct Vec3 {
    float x, y, z;
};

struct Texture;

// Projected screen point.
struct ScreenPt {
    int32_t x, y;
};

// Vertex of a textured triangle handed to the rasterisers (0x1c bytes).
struct TexVert {
    float x, y;            // screen position
    float u, v;            // texel coordinates
    float f10, f14, f18;
};

// One face of an OMT model (0x4c bytes, stored in the file as-is).
struct ModelFace {
    int32_t type;          // +0x00  3/4 = textured
    int32_t f04[6];        // +0x04
    float u[3];            // +0x1c texture coordinates (0..1)
    float v[3];            // +0x28
    int32_t tu[3];         // +0x34 texel coordinates (after SetTexture)
    int32_t tv[3];         // +0x40
};

// One animation frame of an OMT model: vertex list.
struct ModelFrame {
    uint32_t count;        // +0x00
    Vec3 *verts;           // +0x04
};

// 3D model loaded from an .OMT file (0x44 bytes).
struct Model {
    char magic[4];         // +0x00 "OMT"
    uint32_t version;      // +0x04 >= 0x100
    uint32_t hFrames;      // +0x08
    uint32_t hFaces;       // +0x0c
    uint32_t hVerts;       // +0x10
    uint32_t h14, h18;     // +0x14
    uint32_t numFaces;     // +0x1c
    uint32_t numVerts;     // +0x20
    uint32_t numFrames;    // +0x24
    ModelFace *faces;      // +0x28
    ModelFrame *frames;    // +0x2c
    Texture *tex;          // +0x30
    int32_t f34, f38;      // +0x34
    float scale;           // +0x3c
    int32_t lightRange;    // +0x40 shade levels spanned by the face normals

    void Construct();                                   // 0x40e980
    int AllocFrames(uint32_t n);                        // 0x40e9c0
    int Load(const char *file);                         // 0x40ea70
    int Load(int fd);                                   // 0x40ec00
    void SetTexture(Texture *t);                        // 0x40ed60
    void Free();                                        // 0x40ee00
    // render3d.cpp
    void DrawTexFast(struct TexVert *v);                // 0x40ee90
    void SetLightRange(int n) { lightRange = n; }       // 0x40f660
    void Draw(int sx, int sy, int frame, float angle);  // 0x40f670
    void Render(int frame, float angle);                // 0x40f6d0
    void RenderCut(int frame, float angle, float minY); // 0x40fab0
    void AddFace(int face, int shade, float nz = 1.0f);                  // 0x40fef0
    void Rasterize();                                   // 0x40ffc0
    void DrawPasses();                                  // (port: hi-res + normal pass)
    void SetOrigin(int x, int y);                       // 0x410090
    void SetScale(float s);                             // 0x4100b0
    void BeginPoly(int type);                           // 0x4100d0
    void SetPolyUV(int32_t *u, int32_t *v);             // 0x4100f0
    void AddPolyVertex(int idx);                        // 0x410130
    struct ScreenPt *Project(struct ScreenPt *out, Vec3 *p); // 0x4101b0
    void EndPoly();                                     // 0x410230
    void FlatPoly();                                    // 0x410250
    int ScanFlatEdges();                                // 0x410270
    void ScanEdge(int *tab, int x0, int y0, int x1, int y1); // 0x410430
    void FillFlatSpans();                               // 0x4104a0
    void ClearFlatSpans();                              // 0x410500
    void TexturedPoly();                                // 0x410550
    void DrawTexClipped(struct TexVert *v);             // 0x4106f0
    void DrawTexTranslucent(struct TexVert *v);         // 0x410b80
    void DrawShadow(int sx, int sy, float wx, float wy, float unused, int frame,
                    float angle, float lightX, float lightY, float lightH); // 0x410f30
    void ShadowFrame(int frame);                        // 0x410fd0
    int ShadowProject(Vec3 *v, struct ScreenPt *out);   // 0x411210
    void ShadowSpans(int32_t *vi);                      // 0x4112d0
    void ShadowApply();                                 // 0x411710
};

// Foreground render list entry (0x4c bytes: 0x44 payload + links).
struct FGObject {
    int32_t type;          // +0x00
    float fx, fy;          // +0x04 world position (models)
    int32_t x0, y0;        // +0x0c screen position
    int32_t x1, y1;        // +0x14
    union {
        Model *model;      // +0x1c
        struct Particle *particle; // (type 0x20)
        void *obj;
    };
    int32_t frame;         // +0x20
    float depth;           // +0x24
    float angle;           // +0x28
    float scale;           // +0x2c
    int32_t light;         // +0x30
    int32_t color;         // +0x34 (colour, or height offset)
    union {
        struct {
            RECT *rect;    // +0x38 receives the drawn area (models, sprites)
            int32_t f3c, f40;
        };
        struct {
            float lightX, lightY, lightH; // +0x38 shadow caster light (type 0x80)
        };
    };
    uint16_t lrgb, crgb;   // port: 555 light colours of 'light' / 'color' (0: grey)
    FGObject *next;        // +0x44
    FGObject *prev;        // +0x48
};

// Particle (blood, smoke, sparks...) (0x3c bytes, 150 at 0x4bbd18).
struct Particle {
    uint8_t state;         // +0x00 0 free, 1 active, 0xff waiting
    float x, y, z;         // +0x04
    float angle;           // +0x10 direction of travel
    uint8_t r, g, b;       // +0x14 colour components
    uint8_t size;          // +0x17 mask shift for the dot
    int32_t color;         // +0x18 RGB555 index into the shade table
    int32_t dot;           // +0x1c dot size in pixels
    float speed;           // +0x20
    float vz;              // +0x24
    float gravity;         // +0x28
    int32_t delay;         // +0x2c
    int32_t life;          // +0x30
    int32_t born;          // +0x34 frame counter at creation
    int32_t lastFrame;     // +0x38

    void Construct() { state = 0; }                                 // 0x412430
    void Init(float x, float y, float z, int dot, int delay);        // 0x412440
    void SetMotion(float speed, float vz, int angle, float gravity); // 0x4124d0
    void Update();                                                   // 0x412500
    void Draw(Shade16 *shade, DirectDrawWindow *ddw);                // 0x4125e0
    float Depth();                                                   // 0x412760
    void SetLife(int n) { life = n; }                                // 0x412780
};

// Animated model set loaded from an .AMT file (0x3c bytes).
struct AnimSet {
    int32_t f00;           // +0x00
    uint32_t count;        // +0x04 number of sequences (models)
    Model *models;         // +0x08
    int32_t f0c, f10;      // +0x0c
    int32_t curIndex;      // +0x14
    Model *cur;            // +0x18
    int32_t defaultIndex;  // +0x1c
    int32_t frame;         // +0x20
    int32_t numFrames;     // +0x24
    int32_t f28, f2c;      // +0x28
    uint8_t mode;          // +0x30  1 once, 2 loop, 4 hold, 0x10 reverse
    Model **outModel;      // +0x34
    int32_t nextIndex;     // +0x38

    void Construct();                                 // 0x40aa20
    void Free();                                      // 0x40aa40
    Model *Loop(uint32_t index);                      // 0x40aa80
    Model *LoopFrom(uint32_t index, uint32_t frame);  // 0x40aac0
    Model *Once(uint32_t index, Model **out, int next); // 0x40ab00
    Model *Hold(uint32_t index);                      // 0x40ab50
    Model *ReverseLoop(uint32_t index);               // 0x40ab90
    Model *Reverse(uint32_t index, Model **out, int next); // 0x40abd0
    int Advance();                                    // 0x40ac10
    int AtEnd();                                      // 0x40aca0
    int Load(const char *file);                       // 0x40acd0
    void SetTexture(Texture *t);                      // 0x40ade0
    void CopyTo(AnimSet *dst);                        // 0x40ae20
    void Share(AnimSet *src, Model **out);            // 0x40ae40
};

// Lightning bolt / spark effect (0xbc bytes).
struct Bolt {
    int32_t life;          // +0x00
    Vec3 a;                // +0x04 start
    Vec3 b;                // +0x10 end
    int32_t segments;      // +0x1c number of points
    Vec3 pts[12];          // +0x20
    Vec3 drift;            // +0xb0

    void Init(Vec3 from, Vec3 to, int n, int life);   // 0x403320
    int IsActive() { return life > 0; }               // 0x403390
    void SetDrift(float x, float y, float z) { drift.x = x; drift.y = y; drift.z = z; } // 0x4033a0
    void Update();                                     // 0x4033d0
    void Draw();                                       // 0x403570
};

struct SpawnRecord;
struct MonsterStats;
struct SpawnDrop;

// A monster instance (0xe0 bytes); 50 per level.
struct Monster {
    float x, y;                // +0x00
    int32_t tileX, tileY;      // +0x08
    uint8_t state;             // +0x10  0x80 = dead / unused
    uint8_t type;              // +0x11
    uint8_t pad12[2];
    float radius;              // +0x14
    uint8_t attitude;          // +0x18  0 = friendly
    uint8_t invisible;         // +0x19
    uint8_t index;             // +0x1a
    uint8_t pad1b;
    int32_t f1c, f20;          // +0x1c
    float oldX, oldY;          // +0x24
    int32_t f2c;               // +0x2c
    AnimSet anim;              // +0x30
    Model *model;              // +0x6c
    int32_t frame;             // +0x70
    int32_t f74;               // +0x74
    MonsterStats *stats;       // +0x78
    RECT rect;                 // +0x7c  screen rect (for mouse picking)
    int32_t hp;                // +0x8c
    uint8_t action;            // +0x90  1 idle, 2 move, 3 attack, 4 dying, 5 wait, 6 alert
    int8_t timer;              // +0x91
    uint8_t aiMode;            // +0x92
    uint8_t pad93;
    int32_t angle;             // +0x94
    int32_t moveAngle;         // +0x98
    int32_t drops[3];          // +0x9c  (SpawnDrop copies)
    float lastSeenX, lastSeenY;// +0xa8
    float destX, destY;        // +0xb0
    int32_t detour;            // +0xb8
    int32_t detourAngle;       // +0xbc
    int32_t angleToPlayer;     // +0xc0
    int32_t savedAngle;        // +0xc4
    int32_t chaseDist;         // +0xc8
    SpawnRecord *spawn;        // +0xcc
    int32_t holdTime;          // +0xd0
    int32_t fearTime;          // +0xd4
    uint8_t rangedTimer;       // +0xd8
    uint8_t deathTimer;        // +0xd9
    uint8_t killedByPlayer;    // +0xda
    uint8_t padDB;
    int32_t xp;                // +0xdc

    void Init(SpawnRecord *s, int idx);        // 0x404790
    void Kill() { deathTimer = 0; state = 0x80; } // 0x404890
    void SetSpawn(SpawnRecord *s);             // 0x4048a0
    void LoadModel();                          // 0x4048d0
    void Update();                             // 0x404900
    void Draw();                               // 0x404a80
    void AIStationary();                       // 0x404c60
    void AIWander();                           // 0x404d40
    int TurnToward();                          // 0x404f30
    void AIGuard();                            // 0x405020
    void Pursue();                             // 0x405090
    void PursueMelee();                        // 0x4050d0
    int Detour();                              // 0x405330
    void PursueSmart();                        // 0x405500
    int ShouldAttack();                        // 0x405940
    void PursueClever();                       // 0x405a20
    void BashDoor();                           // 0x405eb0
    void PursueRanged();                       // 0x405f40
    int ReachedDest();                         // 0x406630
    int FacingPlayer();                        // 0x406670
    void Waiting();                            // 0x4066a0
    void Attacking();                          // 0x406720
    int MoveToDest();                          // 0x4067a0
    void Walking();                            // 0x406850
    void AIFlee();                             // 0x4068f0
    int FleeStep();                            // 0x406950
    void Fleeing();                            // 0x4069d0
    void AIDeathAnim();                        // 0x406aa0
    void AlertAnim();                          // 0x406b00
    int CanSee(float px, float py);            // 0x406b30
    int AngleTo(float px, float py);           // 0x406bf0
    int InAttackRange(float px, float py, int angle); // 0x406c10
    int CanMove(int angle, float step);        // 0x406c60
    int BlockedByMonster(float x, float y);    // 0x406db0
    int TakeHit();                             // 0x406e80
    int ShieldHit();                           // 0x407100
    int ProjectileHit(struct Projectile *p);   // 0x4071b0
    void ApplyProjectile(struct Projectile *p); // 0x407200
    int Die(int byPlayer);                     // 0x407420
    void Remove();                             // 0x407530
    int HitTest(int mx, int my);               // 0x4076c0
    int SpellKill();                           // 0x407700
    void SpellDamage(int dmg);                 // 0x407780
    int SpellWeaken();                         // 0x407870
    void AttackPlayer();                       // 0x407930
    void SpecialAttack();                      // 0x407c90
    int IsVisible();                           // 0x407d80
    void FinishDying();                        // 0x407dd0
    void LeaveCorpse();                        // 0x407e00
    int IsActive();                            // 0x407f00
    void AddDrop(int item, int qty);           // 0x407f20
    void CheckTileTrigger();                   // 0x407f60
    void RegenHp();                            // 0x407fa0
    int IsHostileTarget();                     // 0x408040
    void Regenerate();                         // 0x408080
    uint8_t GetAttitude();                     // 0x403aa0
};

// CD audio player (0x14 bytes). The port plays optional music files instead.
struct CDAudio {
    int32_t f00;
    int32_t track;         // +0x04
    int32_t opened;        // +0x08
    uint16_t deviceId;     // +0x0c
    int32_t mode;          // +0x10
};

// 16-bit (RGB555) texture decoded from an 8-bit PCX file (0x14 bytes).
struct Texture {
    int32_t w;             // +0x00
    int32_t h;             // +0x04
    float fw;              // +0x08 width as float
    float fh;              // +0x0c height as float
    uint16_t *data;        // +0x10

    void Construct();                                   // 0x41d180
    int Load(const char *file);                         // 0x41d190
    static uint8_t *LoadFile(const char *file, uint8_t **pixels, uint8_t **palette); // 0x41d250
    static void SetPalette(const uint8_t *pal);         // 0x41d310
    void SetSize(const uint8_t *pcxHeader);             // 0x41d360
    int Decode(const uint8_t *src);                     // 0x41d3a0
    int WidthShift();                                   // 0x41d470
    void Free();                                        // 0x41d4b0
    int Blit(Texture *src, int x, int y);               // 0x41d4d0
    void GetPtr(uint8_t **ptr, int *pitch);             // 0x41d560
    void ToGrey();                                      // 0x41d580
};



// Debug log file (0x104 bytes).
struct Log {
    char name[0x100];
    FILE *file;            // +0x100
    void Construct();                    // 0x40bc80
    int Open(const char *fname);         // 0x40bc90
    void Close();                        // 0x40bd80
    void Destroy();                      // 0x40bdc0
};

// AVI cut-scene player: see platform/media.h

