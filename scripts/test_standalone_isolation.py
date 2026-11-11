#!/usr/bin/env python3
"""Run relocated native binaries with the entire Wine tree hidden and no network."""
from pathlib import Path
import os,shutil,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='dfreernet-isolated-') as name:
    stage=Path(name);app=stage/'app';shutil.copytree(root/'dist/dfreernet',app)
    shutil.copy2(root/'build-native/rnet_standalone_test',app/'bin/rnet_standalone_test')
    shutil.copy2(root/'tests/fixtures/original-reference-default.R-net',stage/'before.R-net')
    (stage/'config').mkdir();(stage/'data').mkdir();(stage/'runtime').mkdir(mode=0o700)
    command=['bwrap','--ro-bind','/','/','--bind',name,name,'--tmpfs',str(Path.home()/'.wine'),'--unshare-net','--dev','/dev','--proc','/proc','--chdir',name]
    for key,value in {'QT_QPA_PLATFORM':'offscreen','TMPDIR':name,'XDG_CONFIG_HOME':str(stage/'config'),'XDG_DATA_HOME':str(stage/'data'),'XDG_RUNTIME_DIR':str(stage/'runtime'),'DFREERNET_SMOKE_SCREENSHOT':str(stage/'standalone.png')}.items():
        command+=['--setenv',key,value]
    def run(args,expected=0):
        result=subprocess.run(command+list(map(str,args)),text=True,capture_output=True,timeout=30)
        print(result.stdout,end='');print(result.stderr,end='')
        assert result.returncode==expected,(args,result.returncode)
        return result.stdout+result.stderr
    run([app/'bin/rnet_standalone_test'])
    run([app/'bin/dfreernet','--help'])
    run([app/'bin/dfreernet','--quit-after','30'])
    text=run([app/'bin/rnet_config_read','rnetMissing',stage/'snapshot','read'],1)
    assert (stage/'snapshot/manifest.csv').exists() and 'FAILED:' in text
    text=run([app/'bin/rnet_program_controller','rnetMissing',stage/'before.R-net',stage/'before.R-net',stage/'write','--stationary-confirmed','--access-level=2'],1)
    assert 'No parameter changes to write' in text
    shutil.copy2(stage/'standalone.png',root/'analysis/standalone-isolated.png')
    shutil.copy2(stage/'standalone.png-rnd-menu.png',root/'analysis/standalone-rnd-menu.png')
    print('PASS: relocated app, catalog, typed editing, save/reopen, template, menu/texts, RND constants and helper startup work with ~/.wine hidden and network isolated. No physical CAN access.')
