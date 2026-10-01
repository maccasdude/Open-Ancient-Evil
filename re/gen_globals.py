#!/usr/bin/env python3
"""Generate src/game/globals.h / globals.cpp from structs.def + globals.def.

structs.def:  C-like struct definitions using the scalar types below, other
              struct names, pointers (T*), fixed arrays and 'fn' (code ptr).
globals.def:  lines "ADDR TYPE NAME[DIMS] [= override]" (hex addr, no 0x).
              TYPE may be 'cstr' (const char*), 'str' (char array initialised
              from the image), a scalar, a struct name, or a pointer.
Initial values for addresses in the initialised part of .data are read from
the original executable; pointers are resolved to strings or other globals.
"""
import re, struct, sys, os
sys.path.insert(0, os.path.dirname(__file__))
from pe import Bin

EXE = os.path.join(os.path.dirname(__file__), '../orig/rpg.exe')
b = Bin(EXE)
DATA_INIT_END = 0x43a000 + 0x24600

SCALARS = {
    'int8': ('int8_t', 1, 'b'), 'uint8': ('uint8_t', 1, 'B'),
    'int16': ('int16_t', 2, 'h'), 'uint16': ('uint16_t', 2, 'H'),
    'int32': ('int32_t', 4, 'i'), 'uint32': ('uint32_t', 4, 'I'),
    'float': ('float', 4, 'f'), 'double': ('double', 8, 'd'),
    'char': ('char', 1, 'b'), 'cstr': ('const char *', 4, 'I'),
    'fn': ('void *', 4, 'I'),
}

class Field:
    def __init__(s, t, name, dims, off):
        s.t, s.name, s.dims, s.off = t, name, dims, off

STRUCT_PACK = {}
structs = {}      # name -> (fields, size, raw_c_body_or_None)
EXTERN = set()
struct_order = []

def type_size(t):
    if t.startswith('obj:'): return int(t.split('@')[1], 0)
    if t.endswith('*'): return 4
    if t == 'char': return 1
    if t in SCALARS: return SCALARS[t][1]
    if t in structs: return structs[t][1]
    raise Exception('unknown type ' + t)

def type_align(t):
    if t.endswith('*'): return 4
    if t in SCALARS: return min(SCALARS[t][1], 4) if t != 'double' else 8
    if t in structs: return structs[t][2]
    return 4

def ctype(t):
    if t.startswith('obj:'): return t[4:].split('@')[0]
    if t.endswith('*'):
        base = t[:-1].strip()
        if base in SCALARS: return SCALARS[base][0] + ' *'
        return base + ' *'
    if t in SCALARS: return SCALARS[t][0]
    return t

def parse_structs(path):
    txt = open(path).read()
    txt = re.sub(r'//[^\n]*', '', txt)
    for m in re.finditer(r'(extern\s+)?struct\s+(\w+)\s*(?:\(\s*pack\s*(\d+)\s*\))?\s*\{(.*?)\};', txt, re.S):
        ext, name, pack, body = m.group(1), m.group(2), m.group(3), m.group(4)
        if ext: EXTERN.add(name)
        pack = int(pack) if pack else 4
        STRUCT_PACK[name] = pack
        structs[name] = ([], 0, 1)  # placeholder for self pointers
        fields = []
        off = 0
        maxal = 1
        for decl in body.split(';'):
            decl = decl.strip()
            if not decl: continue
            decl = re.sub(r'\s*\*\s*', '* ', decl)
            while '* *' in decl: decl = decl.replace('* *', '**')
            mm = re.match(r'([\w:@]+\**)\s+(.+)$', decl)
            if not mm: raise Exception('bad decl ' + decl)
            t = mm.group(1).replace(' ', '')
            for nm in mm.group(2).split(','):
                nm = nm.strip()
                md = re.match(r'(\w+)((?:\[[^\]]+\])*)$', nm)
                fname = md.group(1)
                dims = [int(x, 0) for x in re.findall(r'\[([^\]]+)\]', md.group(2))]
                al = min(type_align(t), pack)
                off = (off + al - 1) // al * al
                fields.append(Field(t, fname, dims, off))
                n = 1
                for d in dims: n *= d
                off += type_size(t) * n
                maxal = max(maxal, al)
        size = (off + maxal - 1) // maxal * maxal
        structs[name] = (fields, size, maxal)
        struct_order.append(name)

def has_pointer(n, seen=None):
    fields = structs[n][0]
    for f in fields:
        t = f.t
        if t.endswith('*') or t in ('cstr', 'fn') or t.startswith('obj:'): return True
        if t in structs and t != n and has_pointer(t): return True
    return False

globs = []   # (addr, type, name, dims, override)
PAD = {}     # name -> extra elements allocated past the original array ("+N")
def parse_globals(path):
    for line in open(path):
        line = line.split('#')[0].strip()
        if not line: continue
        mp = re.search(r'\s\+(\d+)$', line)
        if mp:
            line = line[:mp.start()]
            PAD[re.match(r'\S+\s+\S+\s+(\w+)', line).group(1)] = int(mp.group(1))
        line = re.sub(r'\s*\*\s*', '* ', line)
        while '* *' in line: line = line.replace('* *', '**')
        m = re.match(r'([0-9a-fA-F]+)\s+([\w:@]+\**)\s+(\w+)((?:\[[^\]]+\])*)\s*(?:=\s*(.*))?$', line)
        if not m: raise Exception('bad global line: ' + line)
        addr = int(m.group(1), 16)
        t = m.group(2).replace(' ', '')
        dims = [int(x, 0) for x in re.findall(r'\[([^\]]+)\]', m.group(4))]
        globs.append((addr, t, m.group(3), dims, m.group(5)))

def gsize(t, dims):
    n = 1
    for d in dims: n *= d
    if t == 'str': return n
    return type_size(t) * n

def find_global(addr):
    for a, t, n, dims, ov in globs:
        if a <= addr < a + max(1, gsize(t, dims)):
            return a, t, n, dims
    return None

def cstring_literal(s):
    out = '"'
    for ch in s:
        c = ord(ch)
        if ch == '\\': out += '\\\\'
        elif ch == '"': out += '\\"'
        elif ch == '\n': out += '\\n'
        elif ch == '\r': out += '\\r'
        elif ch == '\t': out += '\\t'
        elif 32 <= c < 127:
            if ch == '?' and out.endswith('?'): out += '\\?'
            else: out += ch
        else: out += '\\x%02x""' % c
    return out + '"'

def read_cstr(a):
    bs = b''
    while True:
        c = b.rd(a, 1)
        if c == b'\0' or len(bs) > 4000: break
        bs += c; a += 1
    return bs.decode('latin-1')

def ptr_expr(v, t):
    if v == 0: return 'nullptr'
    base = t[:-1] if t.endswith('*') else None
    if t == 'cstr' or base in ('char', 'int8', 'uint8'):
        g = find_global(v)
        if g and g[1] == 'str':
            return '%s%s' % (g[2], '' if v == g[0] else ' + %d' % (v - g[0]))
        return cstring_literal(read_cstr(v))
    if t == 'fn':
        return '(void *)0x%x /* code */' % v
    g = find_global(v)
    if not g:
        return '(%s)0x%x /* UNRESOLVED */' % (ctype(t), v)
    a, gt, gn, gd = g
    off = v - a
    esz = gsize(gt, gd[1:]) if gd else gsize(gt, [])
    if gt.startswith('obj:'): gt = gt[4:].split('@')[0]
    if gd and off % esz == 0 and (base == gt):
        return '&%s[%d]' % (gn, off // esz)
    if off == 0 and base == gt:
        return '&%s' % gn
    return '(%s)((char *)&%s + %d)' % (ctype(t), gn, off)

def value_init(t, a):
    """Initializer for a single object of type t at address a."""
    if t.endswith('*') or t in ('cstr', 'fn'):
        return ptr_expr(b.u32(a) if a < DATA_INIT_END else 0, t)
    if t in SCALARS:
        ct, sz, fmt = SCALARS[t]
        if a >= DATA_INIT_END: return '0'
        v = struct.unpack('<' + fmt, b.rd(a, sz))[0]
        if t == 'float' or t == 'double':
            r = repr(v)
            if r in ('inf', '-inf', 'nan'): return '0'
            return r + ('f' if t == 'float' else '')
        return str(v)
    if t in structs:
        fields, size, _ = structs[t]
        parts = [array_init(f.t, f.dims, a + f.off) for f in fields]
        return '{' + ', '.join(parts) + '}'
    raise Exception('type ' + t)

def array_init(t, dims, a):
    if not dims: return value_init(t, a)
    esz = gsize(t, dims[1:])
    return '{' + ', '.join(array_init(t, dims[1:], a + i * esz) for i in range(dims[0])) + '}'

def is_zero(a, n):
    if a >= DATA_INIT_END: return True
    n = min(n, DATA_INIT_END - a)
    return b.rd(a, n) == b'\0' * n

def main():
    re_dir = os.path.dirname(__file__)
    parse_structs(os.path.join(re_dir, 'structs.def'))
    parse_globals(os.path.join(re_dir, 'globals.def'))
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(re_dir, '../port/src/game')
    h = ['// Generated by re/gen_globals.py - do not edit.', '#pragma once', '#include <stdint.h>', '#include "../engine/ddw16.h"', '#include "types.h"', '']
    for n in struct_order:
        if n not in EXTERN: h.append('struct %s;' % n)
    h.append('')
    for n in struct_order:
        if n in EXTERN: continue
        fields, size, _ = structs[n]
        pk = STRUCT_PACK.get(n, 4)
        if pk < 4: h.append('#pragma pack(push, %d)' % pk)
        h.append('struct %s { // original size 0x%x' % (n, size))
        for f in fields:
            h.append('    %s %s%s; // +0x%x' % (ctype(f.t), f.name, ''.join('[%d]' % d for d in f.dims), f.off))
        h.append('};')
        if pk < 4: h.append('#pragma pack(pop)')
        if not has_pointer(n):
            h.append('static_assert(sizeof(%s) == 0x%x, "%s must keep the original layout");' % (n, size, n))
        h.append('')
    c = ['// Generated by re/gen_globals.py - do not edit.', '#include "globals.h"', '']
    for a, t, n, dims, ov in sorted(globs):
        dimstr = ''.join('[%d]' % d for d in dims)
        if n in PAD: dimstr = '[%d]' % (dims[0] + PAD[n]) + ''.join('[%d]' % d for d in dims[1:])
        if t == 'str':
            h.append('extern char %s%s; // 0x%x' % (n, dimstr, a))
            size = gsize(t, dims)
            if is_zero(a, size):
                c.append('char %s%s; // 0x%x' % (n, dimstr, a))
            else:
                raw = b.rd(a, min(size, max(0, DATA_INIT_END - a)))
                c.append('char %s%s = {%s}; // 0x%x' % (n, dimstr, ','.join(str(x) for x in raw), a))
            continue
        h.append('extern %s %s%s; // 0x%x' % (ctype(t), n, dimstr, a))
        if t.startswith('obj:'):
            c.append('%s %s%s; // 0x%x' % (ctype(t), n, dimstr, a))
            continue
        if ov is not None:
            c.append('%s %s%s = %s; // 0x%x' % (ctype(t), n, dimstr, ov, a))
        elif is_zero(a, gsize(t, dims)) and not (t.endswith('*') or t == 'cstr'):
            c.append('%s %s%s; // 0x%x' % (ctype(t), n, dimstr, a))
        else:
            c.append('%s %s%s = %s; // 0x%x' % (ctype(t), n, dimstr, array_init(t, dims, a), a))
    open(os.path.join(out, 'globals.h'), 'w').write('\n'.join(h) + '\n')
    open(os.path.join(out, 'globals.cpp'), 'w').write('\n'.join(c) + '\n')
    # overlap check
    s = sorted(globs)
    for i in range(len(s) - 1):
        a, t, n, d, _ = s[i]
        if a + gsize(t, d) > s[i + 1][0]:
            print('WARNING overlap: %s (0x%x+0x%x) and %s (0x%x)' % (n, a, gsize(t, d), s[i + 1][2], s[i + 1][0]))
    print('%d structs, %d globals' % (len(structs), len(globs)))

if __name__ == '__main__':
    main()
