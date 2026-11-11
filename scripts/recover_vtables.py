#!/usr/bin/env python3
"""Read-only MSVC RTTI/vtable inventory for targeted database types."""
import json,re,struct
from pathlib import Path
from analyze_pe import PE
root=Path(__file__).resolve().parents[2];p=PE(root/'RNet Programmer6 OEM.exe')
def va(o):
 for s in p.sections:
  if s['offset']<=o<s['offset']+s['size']:return p.base+s['rva']+o-s['offset']
 return 0
def refs(v):
 pat=struct.pack('<I',v);start=0
 while True:
  o=p.b.find(pat,start)
  if o<0:return
  yield o;start=o+1
out=[]
terms=['Repository','ProgrammerDoc','FileDetails','FileData','CryptByte','ParameterMap','DataBaseData','DepMap','Dependency','LayoutList','LayoutItem','ParmMapBase','Organisations','DependencyMap','NVLayoutList','PreDefEditMap','SystemMap','TreeNodeList','FilenameMap','ProductInformation','ModalProcessInfo','Version','ErrorCodeDictionary','RTFKeyedData','Languages','FileSetList','HardwareIDCollection','FirmwareIDCollection','ModuleTypes','VendorVariants','DefaultDatadBase','AssgnBtnValues','ShortcutValues','Dictionary','Parameter','DataType','FileLayout','Assign','TreeNode','LayoutItem','Texts','DefaultData']
for match in re.finditer(rb'\.\?AV([^\x00]{1,200})@@\x00',p.b):
 name=match.group(1).decode('ascii',errors='replace')
 if not any(t in name for t in terms) or name.startswith('CXTP'):continue
 td=va(match.start()-8)
 for tr in refs(td):
  co=tr-12
  if co<0 or p.u32(co)!=0:continue
  for vref in refs(va(co)):
   funcs=[p.u32(vref+4+i*4) for i in range(4)]
   if not all(0x401000<=f<0x632400 for f in funcs):continue
   out.append(dict(name=name,vtable=hex(va(vref+4)),functions=[f'{f:08x}' for f in funcs]))
(root/'reverse-engineering/analysis/vtables.json').write_text(json.dumps(out,indent=2))
for row in out:
 if row['name'] in ['C'+x for x in terms]:print(row['name'],row['functions'][3])
