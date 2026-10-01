import pefile, struct, re, json, os
from capstone import *
from capstone.x86 import *
class Bin:
    def __init__(s, path):
        s.pe=pefile.PE(path); s.base=s.pe.OPTIONAL_HEADER.ImageBase
        s.img=bytearray(s.pe.get_memory_mapped_image())
        s.secs={}
        for sec in s.pe.sections:
            n=sec.Name.rstrip(b'\0').decode(); s.secs[n]=(s.base+sec.VirtualAddress, s.base+sec.VirtualAddress+max(sec.Misc_VirtualSize,sec.SizeOfRawData))
        s.imports={}
        for e in getattr(s.pe,'DIRECTORY_ENTRY_IMPORT',[]):
            for i in e.imports:
                s.imports[i.address]= e.dll.decode().split('.')[0]+'!'+(i.name.decode() if i.name else '#%d'%i.ordinal)
        s.exports={}
        if hasattr(s.pe,'DIRECTORY_ENTRY_EXPORT'):
            for e in s.pe.DIRECTORY_ENTRY_EXPORT.symbols:
                s.exports[s.base+e.address]=e.name.decode() if e.name else '#%d'%e.ordinal
        s.md=Cs(CS_ARCH_X86,CS_MODE_32); s.md.detail=True
        s.tb,s.te=s.secs['.text']
    def rd(s,a,n): return bytes(s.img[a-s.base:a-s.base+n])
    def u32(s,a): return struct.unpack('<I',s.rd(a,4))[0]
    def insec(s,a,name):
        b,e=s.secs.get(name,(0,0)); return b<=a<e
    def valid(s,a): return s.base<=a<s.base+len(s.img)
    def cstr(s,a,maxn=200):
        if not s.valid(a): return None
        b=s.rd(a,maxn); z=b.find(b'\0')
        if z<3: return None
        t=b[:z]
        if all(32<=c<127 or c in (9,10,13) for c in t): return t.decode()
        return None
def demangle(n):
    if not n.startswith('?'): return n.lstrip('_').split('@')[0]
    if n.startswith('??0'): p=n[3:].split('@'); return '%s::%s'%(p[0],p[0])
    if n.startswith('??1'): p=n[3:].split('@'); return '%s::~%s'%(p[0],p[0])
    if n.startswith('??4'): p=n[3:].split('@'); return '%s::operator='%(p[0])
    p=n[1:].split('@'); r='%s::%s'%(p[1],p[0])
    if r in OVERLOADED: r+='_'+n.split('@@')[1][3:].rstrip('@Z').replace('AAV','').replace('@','')
    return r
OVERLOADED={'CSprite16::Draw','CSprite16::DrawLit','CSprite16::DrawDithered','CSprite16::DrawWhite','CSprite16::DrawLitLeftToRight','DirectDrawWindow::FillRect','Text::Print','Text::PrintC','Text::PrintS','PCX::Display','Sprite::Init','SpriteTable::Save','Shade16::Init','Font::Font','PCX::PCX'}
