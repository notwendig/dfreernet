#!/usr/bin/env python3
"""Open the user's indoor document and retain a screenshot of this application."""
from pathlib import Path
import subprocess
import time
root=Path(__file__).resolve().parents[1]
with (root/'analysis/user-values-session.log').open('ab') as log:
    process=subprocess.Popen([str(root/'scripts/run-wine.sh'),'--document',r'C:\R-net Data\CS16110708-indoor.R-net'],stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
end=time.monotonic()+45
while True:
    windows=subprocess.run(['wmctrl','-l'],capture_output=True,text=True,check=True).stdout
    matches=[line.split()[0] for line in windows.splitlines() if 'CS16110708-indoor.R-net' in line and 'R-net C++ Rekonstruktion' in line]
    if matches:
        assert len(matches)==1,'More than one preview window; refuse choosing arbitrarily'
        window=matches[0]
        subprocess.run(['xdotool','windowactivate','--sync',window],check=True,timeout=10)
        subprocess.run(['import','-window',window,str(root/'analysis/user-values-editor.png')],check=True,timeout=15)
        print('Opened indoor document; screenshot saved; application remains open. PID:',process.pid)
        break
    assert process.poll() is None,'Reconstruction exited before preview'
    assert time.monotonic()<end,'Preview window not observed'
    time.sleep(.25)
