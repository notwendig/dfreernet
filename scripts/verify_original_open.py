#!/usr/bin/env python3
"""Open the generated edited fixture in the isolated original, then save a new copy."""
from pathlib import Path
import subprocess
import time
import sys
root=Path(__file__).resolve().parents[1]
source=root/'build-native/cpp-edited-values.R-net'
target=Path('/tmp/rnet-wine-isolated/drive_c/original-resaved-edited.R-net')
assert source.is_file() and not target.exists()
command=[str(root/'scripts/isolated-wine.sh'),'wine',str(root/'build-windows/rnet_observe.exe')]
def run(*args):
    p=subprocess.run([*command,*args],capture_output=True,text=True,timeout=20)
    assert p.returncode==0,(p.returncode,p.stdout,p.stderr)
    return p.stdout
def wait_for(pattern):
    end=time.monotonic()+45
    while True:
        output=run()
        if pattern in output:return output
        assert time.monotonic()<end,(pattern,output)
        time.sleep(.25)
if '--inspect' in sys.argv:
    print(run());raise SystemExit
run('57601');wait_for('title=Open')
run('open-path','Z:'+str(source).replace('/','\\'))
print(wait_for('title=cpp-edited-values.R-net - PGDT Rnet Programmer'))
run('57604');wait_for('title=Save As')
run('save-path','C:\\original-resaved-edited.R-net')
wait_for('title=original-resaved-edited.R-net - PGDT Rnet Programmer')
end=time.monotonic()+10
while not target.is_file():
    assert time.monotonic()<end
    time.sleep(.1)
print('PASS: isolated original opened the C++-edited document and saved a new copy:',target)
