#!/usr/bin/env python3
from pathlib import Path
import hashlib,sqlite3,zlib
root=Path(__file__).resolve().parents[1]
with sqlite3.connect(f'file:{root/"resources/dfreernet.sqlite3"}?mode=ro',uri=True) as db:
    assert db.execute('pragma integrity_check').fetchone()==('ok',)
    assert not db.execute('pragma foreign_key_check').fetchall()
    assert db.execute('pragma user_version').fetchone()==(1,)
    for table,count in [('parameters',2154),('storage_maps',29),('editors',164),('resource_texts',1454),('parameter_fields',2154*30),('parameter_texts',2154*12),('editor_options',164*33)]:
        assert db.execute('select count(*) from '+table).fetchone()==(count,),table
    assert db.execute('select text from parameter_texts where parameter_key=68 and kind=0 and ordinal=2').fetchone()==('OBP Keycode Entry',)
    packed=db.execute("select data from asset_blobs where name='catalog'").fetchone()[0]
    assert hashlib.sha256(zlib.decompress(packed[4:])).hexdigest()=='30bbe3a7edc45b1ec7975544254bbec6853199a899f46144b33eeebe946723cc'
    assert db.execute('select count(*) from storage_maps where length(map_key)>19 or (length(map_key)=19 and map_key>\'9223372036854775807\')').fetchone()[0]>0
print('SQLite-Tabellen, Integrität und Referenzdaten geprüft.')
