#!/usr/bin/env python3
"""Create a new independent private project; refuse any existing destination."""
from pathlib import Path
import datetime,hashlib,json,shutil,subprocess,sys
root=Path(__file__).resolve().parents[1]
if len(sys.argv)!=2:raise SystemExit('Usage: create_standalone_project.py NEW_DIRECTORY')
target=Path(sys.argv[1]).absolute()
if target.exists():raise SystemExit('Destination exists; refusing to overwrite it')
target.mkdir(parents=True,exist_ok=False)
for folder in ['src','scripts','tests','cmake','translations','resources','docs']:
    shutil.copytree(root/folder,target/folder,ignore=shutil.ignore_patterns('__pycache__'))
for filename in ['CMakeLists.txt','.gitignore','README.md','README.de.md','CHANGELOG.md','CHANGELOG.de.md','originals.sha256']:
    shutil.copy2(root/filename,target/filename)
(target/'analysis').mkdir();shutil.copytree(root/'dist/dfreernet',target/'dist/dfreernet')
# Rebuild in place; this validates relocation and regenerates a clean CMake cache.
with (target/'analysis/standalone-install-tests.log').open('w') as log:
    subprocess.run([str(target/'scripts/test.sh')],cwd=target,stdout=log,stderr=subprocess.STDOUT,check=True)
with (target/'analysis/standalone-package.log').open('w') as log:
    subprocess.run([str(target/'scripts/package.sh')],cwd=target,stdout=log,stderr=subprocess.STDOUT,check=True)
exe=target/'build-native/dfreernet'
state={'updated_at':datetime.datetime.now().astimezone().isoformat(),'objective_complete':False,'current_task_complete':True,'current_stage':'Standalone native Qt app with SQLite catalog and File / Import RND catalog','exe':str(exe),'exe_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'original_installation_required':False,'embedded_database':'resources/dfreernet.sqlite3','historical_workspace':str(root),'last_test_log':'analysis/standalone-install-tests.log','tests_last_recorded':'Offline CTest suite passed; rebuilt independently in this directory','controller_operations_during_install':False,'data_provenance':'resources/PROVENANCE.md','import_scope':'New SQLite file only; does not activate an imported catalog or change controller data'}
(target/'CHECKPOINT.json').write_text(json.dumps(state,ensure_ascii=False,indent=2)+'\n')
files=[]
for folder in ['src','scripts','tests','cmake','translations','resources','docs']:
    files.extend(p for p in (target/folder).rglob('*') if p.is_file() and '__pycache__' not in p.parts)
files.extend(target/name for name in ['CMakeLists.txt','.gitignore','README.md','README.de.md','CHANGELOG.md','CHANGELOG.de.md','originals.sha256','CHECKPOINT.json'])
(target/'QUELLSTAND.sha256').write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+str(p.relative_to(target))+'\n' for p in sorted(files)))
print('Independent project installed and verified: '+str(target))
