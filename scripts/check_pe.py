#!/usr/bin/env python3
"""Independent build artifact check; no application logic."""
import pathlib,sys
from analyze_pe import PE
p=PE(sys.argv[1]);imports=p.imports();byname={i['dll'].lower():i for i in imports}
assert p.u16(p.pe+4)==0x14c and p.u16(p.op)==0x10b
assert p.u16(p.op+68)==2, 'Expected GUI subsystem'
assert p.u32(p.op+116)>0, 'Manifest and version resources missing'
assert 'ftd2xx.dll' not in byname, 'Direct CAN application must not require FTDI at startup'
assert 'ws2_32.dll' in byname, 'Direct CAN loopback transport missing'
assert not any('pthread' in i['dll'].lower() or 'libstdc++' in i['dll'].lower() or 'libgcc' in i['dll'].lower() for i in imports)
assert not list(pathlib.Path(sys.argv[1]).parent.glob('*.dll')), 'Unexpected DLL next to executable'
print('PASS: PE32 i386 GUI; direct Winsock transport; no mandatory FTDI or replacement/C++ runtime DLLs')
print('Static imports (not proof of runtime loading): '+', '.join(i['dll'] for i in imports))
