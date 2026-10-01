// Vector helpers, sine/cosine tables and angles (0x40e030-0x40e4e0).
// Angles are in degrees; the tables hold 3600 entries (0.1 degree steps).
// Screen/world convention: angle 0 points to -y, 90 to +x.
#include "game.h"

static float gCosTable[3600];   // 0x4b1908
static float gSinTable[3600];   // 0x4b5148

// 0x40e030
void Normalize(Vec3 *v)
{
    float x = v->x, y = v->y, z = v->z;
    double len = sqrt((double)z * z + (double)y * y + (double)x * x);
    if (len == 0.0) len = 1.0;
    v->x = (float)(v->x / len);
    v->y = (float)(v->y / len);
    v->z = (float)(v->z / len);
}

// 0x40e0a0: unit normal of the triangle p0,p1,p2: (p0 - p2) x (p2 - p1).
void TriangleNormal(Vec3 *out, Vec3 *p0, Vec3 *p1, Vec3 *p2)
{
    float a = p2->x - p1->x, b = p2->y - p1->y, c = p2->z - p1->z;
    float d = p0->x - p2->x, e = p0->y - p2->y, f = p0->z - p2->z;
    Vec3 n;
    n.x = c * e - b * f;
    n.y = a * f - c * d;
    n.z = b * d - a * e;
    Normalize(&n);
    *out = n;
}

// 0x40e170: build the tables. The original steps a double by 0.1 and
// truncates a*10 for the index, so reproduce that exactly.
void InitTrigTables()
{
    double a = 0.0;
    do {
        double r = a * 6.283185307 * 0.002777777777777778;
        int idx = (int)(a * 10.0);
        double s = sin(r), c = cos(r);
        a -= -0.1;
        if (idx >= 0 && idx < 3600) {
            gSinTable[idx] = (float)s;
            gCosTable[idx] = (float)c;
        }
    } while (a < 360.0);
}

static inline int WrapIndex(int i)
{
    if (i < 0) i += (int)((unsigned)(0xe0f - i) / 3600u) * 3600;
    if (i >= 3600) i %= 3600;
    return i;
}

// 0x40e1d0
float CosF(float deg) { return gCosTable[WrapIndex((int)((double)deg * 10.0))]; }
// 0x40e230
float SinF(float deg) { return gSinTable[WrapIndex((int)((double)deg * 10.0))]; }
// 0x40e290
float CosDeg(int deg) { return gCosTable[WrapIndex(deg * 10)]; }
// 0x40e2e0
float SinDeg(int deg) { return gSinTable[WrapIndex(deg * 10)]; }

// 0x40e330: rotate about the vertical axis.
void RotateY(Vec3 *in, Vec3 *out, float deg)
{
    float x = in->x, z = in->z;
    float c = CosF(deg);
    float s = SinF(deg);
    out->x = c * x + s * z;
    out->z = c * z - s * x;
    out->y = in->y;
}

// 0x40e3b0: tilt by 30 degrees about the x axis (isometric view).
void IsoTilt(Vec3 *in, Vec3 *out)
{
    float y = in->y, z = in->z;
    out->y = y * 0.86602f + z * 0.5f;
    out->x = in->x;
    out->z = z * 0.86602f - y * 0.5f;
}

// 0x40e400: compass angle (0 = up/-y, 90 = +x) from (x0,y0) to (x1,y1).
int AngleBetween(float x0, float y0, float x1, float y1)
{
    float dx = x1 - x0;
    float dy = y1 - y0;
    if (y1 == y0) return x0 < x1 ? 90 : 270;
    if (x1 == x0) return y0 < y1 ? 180 : 0;
    int a = (int)(atan((double)dx / (double)dy) * 360.0 * 0.15915494309644432);
    if (dy <= 0.0f)
        a += 180;
    else if (dx <= 0.0f)
        a += 360;
    a = 180 - a;
    if (a < 0) a += 360;
    return a;
}
