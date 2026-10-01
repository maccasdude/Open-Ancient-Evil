// Minimal Win32 type compatibility for the Ancient Evil Linux port.
#pragma once
#include <stdint.h>
#include <stddef.h>

typedef int32_t BOOL;
typedef uint32_t DWORD;
typedef uint16_t WORD;
typedef uint8_t BYTE;
typedef int32_t LONG;
typedef uint32_t UINT;
typedef void *HANDLE;
typedef void *HWND;
typedef void *HINSTANCE;

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

struct RECT {
    int32_t left, top, right, bottom;
};

struct POINT {
    int32_t x, y;
};

static inline void SetRect(RECT *r, int l, int t, int rr, int b) {
    r->left = l; r->top = t; r->right = rr; r->bottom = b;
}

// MSVC _itoa
static inline char *win_itoa(int v, char *buf, int radix)
{
    char tmp[40];
    unsigned u = (radix == 10 && v < 0) ? (unsigned)-(long long)v : (unsigned)v;
    int n = 0;
    do {
        int d = (int)(u % (unsigned)radix);
        tmp[n++] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
        u /= (unsigned)radix;
    } while (u);
    char *p = buf;
    if (radix == 10 && v < 0) *p++ = '-';
    while (n) *p++ = tmp[--n];
    *p = 0;
    return buf;
}
