#!/usr/bin/env python3
"""Read-only PE32 inventory. Python is analysis tooling, not application logic."""
import collections, hashlib, json, pathlib, struct, sys
class PE:
    def __init__(self, path):
        self.path=pathlib.Path(path); self.b=self.path.read_bytes()
        self.u16=lambda o:struct.unpack_from('<H',self.b,o)[0]
        self.u32=lambda o:struct.unpack_from('<I',self.b,o)[0]
        assert self.b[:2]==b'MZ'
        self.pe=self.u32(60); assert self.b[self.pe:self.pe+4]==b'PE\0\0'
        self.op=self.pe+24; assert self.u16(self.op)==0x10b
        self.base=self.u32(self.op+28)
        self.sections=[]
        for i in range(self.u16(self.pe+6)):
            o=self.op+self.u16(self.pe+20)+i*40
            self.sections.append(dict(name=self.b[o:o+8].rstrip(b'\0').decode(),vsize=self.u32(o+8),rva=self.u32(o+12),size=self.u32(o+16),offset=self.u32(o+20)))
    def off(self,r):
        if r<self.u32(self.op+60):return r
        for s in self.sections:
            if s['rva']<=r<s['rva']+max(s['size'],s['vsize']):return s['offset']+r-s['rva']
        raise ValueError(hex(r))
    def asc(self,o):return self.b[o:self.b.index(0,o)].decode('ascii',errors='replace')
    def imports(self):
        o=self.off(self.u32(self.op+104)); out=[]
        while self.u32(o+12):
            name=self.asc(self.off(self.u32(o+12))); t=self.off(self.u32(o) or self.u32(o+16)); syms=[]; n=0
            while self.u32(t):
                v=self.u32(t); syms.append(dict(iat=hex(self.base+self.u32(o+16)+n*4),symbol=('#'+str(v&65535)) if v>>31 else self.asc(self.off(v)+2)))
                t+=4;n+=1
            out.append(dict(dll=name,symbols=syms));o+=20
        return out
    def exports(self):
        r=self.u32(self.op+96)
        if not r:return []
        o=self.off(r); base=self.u32(o+16); f=self.off(self.u32(o+28)); n=self.off(self.u32(o+32)); ords=self.off(self.u32(o+36)); out=[]
        for i in range(self.u32(o+24)):
            idx=self.u16(ords+i*2);out.append(dict(name=self.asc(self.off(self.u32(n+i*4))),ordinal=base+idx,address=hex(self.base+self.u32(f+idx*4))))
        return out
    def resources(self):
        root=self.off(self.u32(self.op+112));out=[]
        def walk(o,path):
            for i in range(self.u16(o+12)+self.u16(o+14)):
                n,v=struct.unpack_from('<II',self.b,o+16+i*8)
                if n>>31:
                    t=root+(n&0x7fffffff);l=self.u16(t); n=self.b[t+2:t+2+l*2].decode('utf-16le')
                if v>>31:walk(root+(v&0x7fffffff),path+[n])
                else:
                    t=root+v;r,size=struct.unpack_from('<II',self.b,t);out.append(dict(path=path+[n],offset=self.off(r),size=size))
        walk(root,[]);return out

def wstr(b,o):
    start=o
    while b[o:o+2]!=b'\0\0':o+=2
    return b[start:o].decode('utf-16le',errors='replace'),o+2

def menu(b):
    ver,offset=struct.unpack_from('<HH',b); out=[]
    if ver!=0:return [dict(unsupported_version=ver)]
    def items(o,depth):
        while o<len(b):
            flags=struct.unpack_from('<H',b,o)[0];o+=2;ident=None
            if not flags&16:ident=struct.unpack_from('<H',b,o)[0];o+=2
            label,o=wstr(b,o);out.append(dict(depth=depth,id=ident,flags=hex(flags),text=label))
            if flags&16:o=items(o,depth+1)
            if flags&128:break
        return o
    items(4+offset,0);return out

def dialog(b):
    ext=b[:4]==b'\x01\x00\xff\xff'
    if ext:
        _,_,helpid,ex,style,count,x,y,cx,cy=struct.unpack_from('<HHIIIHhhhh',b);o=26
    else:
        style,ex,count,x,y,cx,cy=struct.unpack_from('<IIHhhhh',b);o=18
    def field(o):
        v=struct.unpack_from('<H',b,o)[0]
        if v==65535:return '#'+str(struct.unpack_from('<H',b,o+2)[0]),o+4
        return wstr(b,o)
    m,o=field(o);c,o=field(o);title,o=field(o)
    if style&64:
        o+=6 if ext else 2
        font,o=wstr(b,o)
    controls=[]
    for i in range(count):
        o=(o+3)&~3
        if ext:
            helpid,ex,st,xx,yy,ww,hh,ident=struct.unpack_from('<IIIhhhhI',b,o);o+=24
        else:
            st,ex,xx,yy,ww,hh,ident=struct.unpack_from('<IIhhhhH',b,o);o+=18
        cl,o=field(o);text,o=field(o);extra=struct.unpack_from('<H',b,o)[0];o+=2
        if extra:o+=extra-2
        controls.append(dict(id=ident,cls=cl,text=text,rect=[xx,yy,ww,hh],style=hex(st)))
    return dict(title=title,rect=[x,y,cx,cy],extended=ext,controls=controls)

def main():
    root=pathlib.Path(__file__).resolve().parents[2];dest=root/'reverse-engineering/analysis';dest.mkdir(exist_ok=True)
    for name in ['RNet Programmer6 OEM.exe','DongleInterface.dll','ftd2xx.dll']:
        p=PE(root/name);r=p.resources();d=dict(file=name,sha256=hashlib.sha256(p.b).hexdigest(),machine=hex(p.u16(p.pe+4)),imagebase=hex(p.base),entry=hex(p.base+p.u32(p.op+16)),sections=p.sections,imports=p.imports(),exports=p.exports(),resources=r)
        (dest/(name+'.json')).write_text(json.dumps(d,indent=2))
        if name.endswith('.exe'):
            menus={};dialogs={};strings={}
            for e in r:
                typ,ident,lang=e['path'];b=p.b[e['offset']:e['offset']+e['size']];key=f'{ident}:{lang}'
                try:
                    if typ==4:menus[key]=menu(b)
                    if typ==5:dialogs[key]=dialog(b)
                    if typ==6:
                        o=0
                        for i in range(16):
                            l=struct.unpack_from('<H',b,o)[0];o+=2;s=b[o:o+l*2].decode('utf-16le');o+=l*2
                            if s:strings[str((ident-1)*16+i)]=s
                except Exception as ex:print(key,ex,file=sys.stderr)
            for kind,value in [('menus',menus),('dialogs',dialogs),('strings',strings)]:
                (dest/(kind+'.json')).write_text(json.dumps(value,indent=2,ensure_ascii=False))
            print('resources',len(r),'menus',len(menus),'dialogs',len(dialogs),'strings',len(strings))
if __name__=='__main__':main()
