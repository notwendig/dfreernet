set pagination off
set confirm off
break *0x005fb180
continue
python
import struct
inf=gdb.selected_inferior()
def u32(p):return struct.unpack('<I',inf.read_memory(p,4).tobytes())[0]
db=u32(0x6cda80);info=u32(db+16);address=None
for i in range(u32(info+16)):
    node=u32(u32(info+12)+i*4)
    while node:
        if u32(node)==2587:address=node+4+0x28;break
        node=u32(node+0xec)
    if address is not None:break
assert address is not None
print('PARAMETER_2587_MASK_BEFORE_POSTLOAD',hex(address),u32(address))
gdb.execute('watch -l *(unsigned int*)'+hex(address))
end
continue
printf "PARAMETER_ACCESS_MASK_WRITE\n"
bt 12
info registers rip rsp cs
x/12wx $rsp
x/10i $rip-12
detach
quit
