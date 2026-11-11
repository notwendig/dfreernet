#!/usr/bin/env python3
"""Verify and embed the local SQLite database; no original-installation access."""
from pathlib import Path
import hashlib,sqlite3,struct,sys,zlib
source,output=map(Path,sys.argv[1:])
for line in (source/'SHA256SUMS').read_text().splitlines():
    digest,name=line.split('  ',1)
    if hashlib.sha256((source/name).read_bytes()).hexdigest()!=digest:raise SystemExit('Resource checksum mismatch: '+name)
path=source/'dfreernet.sqlite3'
with sqlite3.connect(f'file:{path}?mode=ro',uri=True) as db:
    assert db.execute('pragma integrity_check').fetchone()==('ok',)
    assert db.execute('pragma user_version').fetchone()==(1,)
raw=path.read_bytes();data=struct.pack('<I',len(raw))+zlib.compress(raw,9)
code='// Generated from the pinned SQLite database. Do not edit.\n#include "runtime_resources.hpp"\n#include <iterator>\nnamespace rnet::assets {\nstatic const unsigned char database_data[]={\n'
code+='\n'.join(','.join(str(v) for v in data[i:i+32])+',' for i in range(0,len(data),32))
code+='\n};\nBytes database(){return Bytes(std::begin(database_data),std::end(database_data));}\n}\n'
output.parent.mkdir(parents=True,exist_ok=True);output.write_text(code)
