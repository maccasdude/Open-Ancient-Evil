// Particle effects: blood, fire, smoke, sparks (0x411870-0x412790).
#include "game.h"

#define LIGHT(x, y) gLightMap[(x) * 65 + (y)]
#define VIS(x, y) gMapVis[(x) * 65 + (y)]
#define TILE(x, y) gLevelMap[gMapRow[(y)] + (x)]

static Particle gParticles[150];                       // 0x4bbd18

static const float kBloodSpeed[4] = {0.05f, 0.1f, 0.15f, 0.2f};              // 0x448bc0
static const float kJitter[9] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f}; // 0x448bd0
static const uint8_t kSplashR[4] = {0xc0, 0x00, 0x80, 0x60};                 // 0x448bf8
static const uint8_t kSplashG[4] = {0x00, 0xc0, 0x80, 0x60};                 // 0x448c00
static const uint8_t kSplashB[4] = {0x00, 0x00, 0x00, 0x60};                 // 0x448c08
static const float kFireSpeed[4] = {0.04f, 0.08f, 0.12f, 0.16f};             // 0x448c10
static const float kSparkSpeed[4] = {0.05f, 0.1f, 0.15f, 0.2f};              // 0x448c20

static inline int Col555(unsigned r, unsigned g, unsigned b)
{
    return (int)((((r & 0xf8) << 5 | (g & 0xf8)) << 2) | (b >> 3));
}

// 0x411880 (static initialiser)
void StaticInit_Particles()
{
    for (int i = 0; i < 150; i++) gParticles[i].Construct();
}

// 0x4118a0: a free particle, or the oldest one.
int FindFreeParticle()
{
    int oldest = gFrameCounter;
    int best = 0;
    for (int i = 0; i < 150; i++) {
        uint8_t s = gParticles[i].state;
        if (s == 0) return i;
        if (s == 1 && gParticles[i].born < oldest) {
            oldest = gParticles[i].born;
            best = i;
        }
    }
    return best;
}

// 0x4118e0
void UpdateParticles()
{
    for (int i = 0; i < 150; i++) gParticles[i].Update();
}

// 0x411900
void ClearParticles()
{
    for (int i = 0; i < 150; i++) gParticles[i].state = 0;
}

// 0x411920
void DrawParticles()
{
    FGObject o;
    memset(&o, 0, sizeof(o));
    o.type = 0x20;
    for (int i = 0; i < 150; i++) {
        Particle *p = &gParticles[i];
        if (p->state != 1) continue;
        o.depth = p->Depth();
        o.particle = p;
        AddFGObject(&o);
    }
}

// 0x411980
void BloodSplat(float x, float y, int angle, int type)
{
    if ((uint8_t)type == 0) return;
    for (int i = 0; i < 7; i++) {
        Particle *p = &gParticles[FindFreeParticle()];
        int delay = rand() % 3;
        p->Init(x, y, 20.0f, rand() % 3 + 2, delay);
        switch ((uint8_t)type) {
        case 1: {   // green
            uint8_t c = (uint8_t)(rand() % 32 + 0x20);
            p->size = 2;
            p->r = 0;
            p->b = 0;
            p->g = c;
            p->color = (c & 0xf8) << 2;
            break;
        }
        case 2: {   // red
            uint8_t c = (uint8_t)(rand() % 32 + 0x60);
            p->size = 4;
            p->b = 0;
            p->g = 0;
            p->r = c;
            p->color = (c & 0xf8) << 7;
            break;
        }
        }
        int a = rand() % 20 + angle - 10;
        float vz = (float)(rand() % 10);
        p->SetMotion(kBloodSpeed[rand() % 4], vz, a, 2.5f);
    }
}

// 0x411ad0: flames left behind by a fireball.
void FireTrail(float x, float y)
{
    for (int i = 0; i < 3; i++) {
        Particle *p = &gParticles[FindFreeParticle()];
        int delay = rand() % 3;
        float py = kJitter[rand() % 9] + y;
        float px = kJitter[rand() % 9] + x;
        p->Init(px, py, 20.0f, 3, delay);
        uint8_t b = (uint8_t)(rand() % 32);
        uint8_t g = (uint8_t)(rand() % 64 + 0x80);
        uint8_t r = (uint8_t)(rand() % 64 + 0x80);
        p->size = 2;
        p->r = r;
        p->g = g;
        p->color = Col555(r, g, b);
        p->b = b;
        p->SetMotion(0.0f, -0.1f, 0, 1.0f);
    }
    Smoke(x, y, 2);
}

// 0x411c10
void Smoke(float x, float y, int count)
{
    for (int i = 0; i < count; i++) {
        Particle *p = &gParticles[FindFreeParticle()];
        int delay = rand() % 3;
        float py = kJitter[rand() % 9] + y;
        float px = kJitter[rand() % 9] + x;
        p->Init(px, py, 20.0f, 4, delay);
        uint8_t c = (uint8_t)(rand() % 60 + 0xb4);
        p->size = 1;
        p->r = c;
        p->b = c;
        p->g = c;
        p->color = Col555(c, c, c);
        p->SetMotion(0.0f, -0.1f, 0, -0.3f);
        p->SetLife(450);
    }
}

// 0x411d00: splash of 'type' coloured drops around (x,y).
void Splash(float x, float y, int type)
{
    for (int i = 0; i < 3; i++) {
        int idx = FindFreeParticle();
        int a = rand() % 360;
        float px = SinDeg(a) * 2.0f + x;
        float py = y - CosDeg(a) * 2.0f;
        Particle *p = &gParticles[idx];
        p->Init(px, py, 100.0f, rand() % 3 + 2, 0);
        if (i != 0) {
            uint8_t b = (uint8_t)(rand() % 128 + 0x40);
            uint8_t r = (uint8_t)(rand() % 128 + 0x40);
            p->size = 2;
            p->b = b;
            p->g = 0;
            p->r = r;
            p->color = ((r & 0xf8) << 7) | (b >> 3);
        } else {
            uint8_t b = (uint8_t)(rand() % 64 + kSplashB[type]);
            uint8_t g = (uint8_t)(rand() % 64 + kSplashG[type]);
            uint8_t r = (uint8_t)(rand() % 64 + kSplashR[type]);
            p->r = r;
            p->size = 2;
            p->b = b;
            p->g = g;
            p->color = Col555(r, g, b);
        }
        p->SetMotion(0.35f, -8.0f, (a + 180) % 360, 0.0f);
        p->SetLife(150);
    }
}

// 0x411ee0
void FireBreath(float x, float y, int angle)
{
    float fx = x - SinDeg(angle) * -0.7f;
    float fy = y - CosDeg(angle) * 0.7f;
    for (int i = 0; i < 8; i++) {
        Particle *p = &gParticles[FindFreeParticle()];
        p->Init(fx, fy, 20.0f, 3, rand() % 3);
        uint8_t b = (uint8_t)(rand() % 32);
        uint8_t g = (uint8_t)(rand() % 64 + 0x80);
        uint8_t r = (uint8_t)(rand() % 64 - 0x40);
        p->size = 6;
        p->g = g;
        p->r = r;
        p->b = b;
        p->color = Col555(r, g, b);
        int a = rand() % 60 + angle - 30;
        float vz = (float)(rand() % 10);
        p->SetMotion(kFireSpeed[rand() % 4], vz, a, 2.5f);
    }
}

// 0x412050: trail of a spit projectile.
void DrawSpit(float x, float y, int angle)
{
    for (int i = 0; i < 5; i++) {
        int idx = FindFreeParticle();
        float px = (float)((rand() % 5) / 10) + x;
        float py = (float)((rand() % 5) / 10) + y;
        float z = (float)(rand() % 10 + 0x28);
        Particle *p = &gParticles[idx];
        p->Init(px, py, z, 3, rand() % 3);
        uint8_t c = (uint8_t)(rand() % 64 + 0x80);
        p->r = c;
        p->b = c;
        p->g = c;
        p->color = Col555(c, c, c);
        int a = rand() % 20 + angle - 10;
        float vz = (float)(rand() % 10);
        p->SetMotion(0.1f, vz, a, 2.5f);
        p->SetLife(150);
    }
}

// 0x4121b0: a slowly rising glow at a tile.
void GroundGlow(int x, int y)
{
    int idx = FindFreeParticle();
    float px = (float)(rand() % 10) * 0.1f + x;
    float py = (float)(rand() % 10) * 0.1f + y;
    float z = (float)(rand() % 10 + 1);
    Particle *p = &gParticles[idx];
    p->Init(px, py, z, 4, rand() % 4);
    uint8_t b = (uint8_t)(rand() % 128 + 0x40);
    uint8_t r = (uint8_t)(rand() % 128 + 0x40);
    p->size = 1;
    p->b = b;
    p->g = 0;
    p->r = r;
    p->color = ((r & 0xf8) << 7) | (b >> 3);
    p->SetMotion(0.0f, 0.2f, 0, -0.1f);
    p->SetLife(600);
}

// 0x4122e0
void Sparks(float x, float y, int angle)
{
    for (int i = 0; i < 10; i++) {
        Particle *p = &gParticles[FindFreeParticle()];
        int delay = rand() % 3;
        p->Init(x, y, 1.0f, rand() % 3 + 2, delay);
        uint8_t b = (uint8_t)(rand() % 64);
        uint8_t g = (uint8_t)(rand() % 64 + 0x80);
        uint8_t r = (uint8_t)(rand() % 64 - 0x40);
        p->size = 1;
        p->g = g;
        p->r = r;
        p->b = b;
        p->color = Col555(r, g, b);
        int a = rand() % 20 + angle - 10;
        float vz = (float)(rand() % 10);
        p->SetMotion(kSparkSpeed[rand() % 4], vz, a, 2.5f);
    }
}

// 0x412440
void Particle::Init(float px, float py, float pz, int dotSize, int wait)
{
    if (VIS((int)px, (int)py) >= 0xfe) return;
    x = px;
    y = py;
    dot = dotSize;
    speed = 0.2f;
    vz = 0.2f;
    z = pz;
    angle = 0;
    if (wait == 0) {
        state = 1;
    } else {
        state = 0xff;
        delay = wait;
    }
    color = 0xffff;
    life = 9999999;
    lastFrame = gFrameCounter;
    born = gFrameCounter;
}

// 0x4124d0
void Particle::SetMotion(float s, float v, int a, float g)
{
    speed = s;
    vz = v;
    angle = (float)a;
    gravity = g;
}

// 0x412500
void Particle::Update()
{
    switch (state) {
    case 0xff:
        if (--delay < 0) state = 1;
        return;
    case 1:
        break;
    default:
        return;
    }
    life += (lastFrame - gFrameCounter) * 25;
    lastFrame = gFrameCounter;
    float s = SinF(angle);
    float c = CosF(angle);
    x = s * speed + x;
    float nx = x;
    y = y - c * speed;
    z = z + vz;
    if (z <= 0.0f) {
        state = 0;
        return;
    }
    if (TILE((int)nx, (int)y) > 0x14 || life <= 0) {
        state = 0;
        return;
    }
    vz = vz - gravity;
}

// 0x4125e0
void Particle::Draw(Shade16 *shade, DirectDrawWindow *ddw)
{
    if (state != 1) return;
    int sx, sy;
    WorldToScreen(x, y, &sx, &sy);
    if (gravity < 0.0f) sx += rand() % 5;
    sx += 0x1f;
    sy -= (int)z;
    shade->SetShadeLevel(LIGHT((int)x, (int)y));
    uint16_t c = shade->current[color];
    if (sy > ViewT() && sy < ViewCap() - 5 && sx > ViewL() && sx < ViewR() - 4)
        DrawDot(sx, sy, dot, ddw->surfacePtr, ddw->pitch, c, size);
}

// 0x4126d0: a dot x dot block ANDed with a darkening mask then ORed with c.
void DrawDot(int x, int y, int n, uint8_t *surf, int pitch, uint16_t c, uint8_t shift)
{
    uint16_t *d = (uint16_t *)(surf + (ptrdiff_t)y * pitch) + x;
    uint8_t m = (uint8_t)(0xff >> shift);
    uint16_t mask = gDDW.MakePixel16(m, m, m);
    if (n == 1) {
        *d = c;
        return;
    }
    uint32_t c2 = ((uint32_t)c << 16) | c;
    uint32_t m2 = ((uint32_t)mask << 16) | mask;
    for (int r = 0; r < n; r++) {
        uint32_t *row = (uint32_t *)(surf + ((ptrdiff_t)y + r) * pitch + x * 2);
        for (int k = (n + 1) >> 1; k; k--) {
            uint32_t v;
            memcpy(&v, row, 4);
            v = (v & m2) | c2;
            memcpy(row, &v, 4);
            row++;
        }
    }
}

// 0x412760
float Particle::Depth()
{
    return ScreenDepth(x, y);
}
