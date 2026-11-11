#!/usr/bin/env python3
"""Developer-only table import; ordinary builds use the pinned SQLite file."""
from pathlib import Path
import json,sqlite3,sys
source,output,assets=map(Path,sys.argv[1:]);data=json.loads(source.read_text())
assert not output.exists(),'Refuse overwriting a database'
legacy=assets/'catalog.deflate'
if legacy.exists():
    blobs={name:(assets/file).read_bytes() for name,file in [('catalog','catalog.deflate'),('document','document-template.deflate'),('seed','blowfish-pi.bin'),('menu_source','menu.json'),('texts_source','texts.json')]}
else:
    with sqlite3.connect(f'file:{assets/"dfreernet.sqlite3"}?mode=ro',uri=True) as old:blobs=dict(old.execute('select name,data from asset_blobs'))
db=sqlite3.connect(output);db.executescript('''
PRAGMA application_id=1145459278;
PRAGMA user_version=1;
PRAGMA foreign_keys=ON;
CREATE TABLE catalog_info(format_version INTEGER PRIMARY KEY CHECK(format_version=1),schema INTEGER,parameter_offset INTEGER,parameter_end INTEGER,storage_map_offset INTEGER,storage_map_end INTEGER);
CREATE TABLE parameters(key INTEGER PRIMARY KEY,id INTEGER,extra84 INTEGER,flag24 INTEGER,signed_storage INTEGER,flag70 INTEGER,flag71 INTEGER,extra8c INTEGER,flagb0 INTEGER);
CREATE TABLE parameter_texts(parameter_key INTEGER REFERENCES parameters(key),kind INTEGER,ordinal INTEGER,text TEXT NOT NULL,PRIMARY KEY(parameter_key,kind,ordinal));
CREATE TABLE parameter_fields(parameter_key INTEGER REFERENCES parameters(key),kind INTEGER,ordinal INTEGER,value INTEGER,PRIMARY KEY(parameter_key,kind,ordinal));
CREATE TABLE storage_maps(map_key TEXT PRIMARY KEY,id INTEGER,version INTEGER,name TEXT NOT NULL);
CREATE TABLE storage_groups(map_key TEXT REFERENCES storage_maps(map_key),group_index INTEGER,instances INTEGER,stride INTEGER,last_section INTEGER,PRIMARY KEY(map_key,group_index));
CREATE TABLE storage_sections(map_key TEXT,group_index INTEGER,section_index INTEGER,field0 INTEGER,field1 INTEGER,field2 INTEGER,field3 INTEGER,offset INTEGER,PRIMARY KEY(map_key,group_index,section_index));
CREATE TABLE storage_items(map_key TEXT,group_index INTEGER,section_index INTEGER,ordinal INTEGER,id INTEGER,width INTEGER,PRIMARY KEY(map_key,group_index,section_index,ordinal));
CREATE TABLE editors(key INTEGER PRIMARY KEY,id INTEGER,flags INTEGER,kind INTEGER,count INTEGER,extra INTEGER,name TEXT NOT NULL,mapped INTEGER);
CREATE TABLE editor_options(editor_key INTEGER REFERENCES editors(key),ordinal INTEGER,label TEXT NOT NULL,value INTEGER,PRIMARY KEY(editor_key,ordinal));
CREATE TABLE file_names(id INTEGER PRIMARY KEY,name TEXT NOT NULL);
CREATE TABLE menus(node INTEGER PRIMARY KEY,parent INTEGER REFERENCES menus(node),ordinal INTEGER,command INTEGER,flags INTEGER,text TEXT NOT NULL);
CREATE INDEX menu_order ON menus(parent,ordinal);
CREATE TABLE resource_texts(ordinal INTEGER PRIMARY KEY,id INTEGER,text TEXT NOT NULL);
CREATE TABLE asset_blobs(name TEXT PRIMARY KEY,data BLOB NOT NULL);
CREATE TABLE provenance(key TEXT PRIMARY KEY,value TEXT NOT NULL);
''')
db.execute('insert into catalog_info values(1,?,?,?,?,?)',data['info'])
for p in data['parameters']:
 db.execute('insert into parameters values(?,?,?,?,?,?,?,?,?)',[p[k] for k in ['key','id','extra84','flag24','signed_storage','flag70','flag71','extra8c','flagb0']])
 for kind,values in enumerate([p['texts'],p['options'],[p['text88']],[p['text90']]]):
  db.executemany('insert into parameter_texts values(?,?,?,?)',[(p['key'],kind,i,v) for i,v in enumerate(values)])
 for kind,values in enumerate([p['metadata'],p['serialized'],p['extension']]):
  db.executemany('insert into parameter_fields values(?,?,?,?)',[(p['key'],kind,i,v) for i,v in enumerate(values)])
for m in data['maps']:
 key=m['key'];db.execute('insert into storage_maps values(?,?,?,?)',(key,m['id'],m['version'],m['name']))
 for gi,g in enumerate(m['groups']):
  db.execute('insert into storage_groups values(?,?,?,?,?)',(key,gi,g['instances'],g['stride'],g['last_section']))
  for si,s in enumerate(g['sections']):
   db.execute('insert into storage_sections values(?,?,?,?,?,?,?,?)',(key,gi,si,*s['fields'],s['offset']))
   db.executemany('insert into storage_items values(?,?,?,?,?,?)',[(key,gi,si,i,*v) for i,v in enumerate(s['items'])])
for e in data['editors']:
 db.execute('insert into editors values(?,?,?,?,?,?,?,?)',[e[k] for k in ['key','id','flags','kind','count','extra','name','mapped']])
 db.executemany('insert into editor_options values(?,?,?,?)',[(e['key'],i,label,e['values'][i]) for i,label in enumerate(e['labels'])])
db.executemany('insert into file_names values(?,?)',data['names'])
next_node=0
def menus(items,parent=None):
 global next_node
 for ordinal,item in enumerate(items):
  next_node+=1;node=next_node
  db.execute('insert into menus values(?,?,?,?,?,?)',(node,parent,ordinal,item['id'],item['flags'],item['text']))
  menus(item['children'],node)
menus(json.loads(blobs['menu_source']))
db.executemany('insert into resource_texts values(?,?,?)',[(i,item['id'],item['text']) for i,item in enumerate(json.loads(blobs['texts_source']))])
db.executemany('insert into asset_blobs values(?,?)',blobs.items())
db.executemany('insert into provenance values(?,?)',[('catalog_sha256','30bbe3a7edc45b1ec7975544254bbec6853199a899f46144b33eeebe946723cc'),('origin','Locally verified schema-33 reference resources; derived data; private use'),('template','Opaque encrypted metadata preserved from reference template; not current controller identity')])
db.commit();assert db.execute('pragma integrity_check').fetchone()==('ok',);assert not db.execute('pragma foreign_key_check').fetchall();db.execute('vacuum');db.close()
print('SQLite-Datenbank mit geprüften Katalogtabellen erstellt.')
