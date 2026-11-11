#!/usr/bin/env python3
"""Analysis experiment only; application decoder is implemented separately in C++."""
from pathlib import Path
import struct,hashlib
from Crypto.Cipher import Blowfish
root=Path(__file__).resolve().parents[2]
m=0xffffffff
state=[];x=0x672d83e1
for i in range(351):x=(x*0x1c8e815-1)&m;state.append(x)
for i in range(351):
 y=(state[i]&0xfff80000)|(state[(i+1)%351]&0x7ffff)
 state[i]=(state[(i+175)%351]^(y>>1)^(0xe4bd75f5 if y&1 else 0))&m
key=[]
for x in state[:31]:
 x^=x>>11;x^=(x&0xcabca5)<<7;x&=m;x^=(x&0xffffffab)<<15;x&=m;x^=x>>17
 key.append(min(254,1+int((x/4294967296.0)*254)))
key=bytes(key)
b=(root/'Generic_V33_1_1245.rnd').read_bytes()
swap=lambda b:b''.join(b[i:i+4][::-1] for i in range(0,len(b),4))
plain=swap(Blowfish.new(key,Blowfish.MODE_ECB).decrypt(swap(b)))
out=root/'reverse-engineering/analysis/rnd-decrypted.bin';out.write_bytes(plain)
print('key sha256',hashlib.sha256(key).hexdigest());print('decrypted head',repr(plain[:100]));print('sha256',hashlib.sha256(plain).hexdigest())
