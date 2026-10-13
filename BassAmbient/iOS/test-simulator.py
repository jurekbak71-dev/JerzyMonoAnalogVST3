"""Bounded simulator boot and smoke test, with diagnostics even if CoreSimulator stalls."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import time

out=Path('simulator-qa');out.mkdir(exist_ok=True)
def run(*args,timeout=90,env=None):
    with (out/'simulator.log').open('a') as log:
        log.write('\nCOMMAND: '+' '.join(args)+'\n');log.flush()
        try:
            p=subprocess.run(args,capture_output=True,text=True,timeout=timeout,env=env)
        except subprocess.TimeoutExpired as e:
            log.write(str(e)+'\n'+str(e.stdout or '')+'\n'+str(e.stderr or ''));raise
        log.write(p.stdout+p.stderr)
        if p.returncode:raise RuntimeError(f'{args}: {p.returncode}\n{p.stdout}\n{p.stderr}')
        return p.stdout.strip()

sim=None
try:
    runtimes=json.loads(run('xcrun','simctl','list','runtimes','-j'))['runtimes']
    available=[r for r in runtimes if r.get('isAvailable') and '.iOS-' in r['identifier']]
    # Prefer the established iOS 18 runtime when available. App deployment minimum is iOS 16.
    stable=[r for r in available if r['version'].startswith('18.')]
    runtime=sorted(stable or available,key=lambda r:tuple(map(int,r['version'].split('.'))))[-1]['identifier']
    sim=run('xcrun','simctl','create','BassAmbient-iPhone14','com.apple.CoreSimulator.SimDeviceType.iPhone-14',runtime)
    run('xcrun','simctl','boot',sim)
    run('xcrun','simctl','bootstatus',sim,'-b',timeout=300)
    run('xcrun','simctl','install',sim,os.environ['APP_PATH'],timeout=120)
    env=dict(os.environ,SIMCTL_CHILD_BASS_AMBIENT_SMOKE='1')
    run('xcrun','simctl','launch',sim,'pl.jerzy.bassambient.mobile',env=env)
    container=Path(run('xcrun','simctl','get_app_container',sim,'pl.jerzy.bassambient.mobile','data'))
    result=container/'Documents/smoke-result.txt'
    deadline=time.monotonic()+120
    while not result.exists() and time.monotonic()<deadline:time.sleep(2)
    shutil.copytree(container/'Documents',out,dirs_exist_ok=True)
    run('xcrun','simctl','io',sim,'screenshot',str(out/'iPhone14-screen.png'))
    assert result.exists(),'No smoke report: inspect simulator.log and screenshots'
    assert result.read_text().strip()=='PASS',result.read_text()
    assert (out/'smoke.wav').stat().st_size>10000
    assert (out/'smoke.mid').stat().st_size>30
    run('xcrun','simctl','terminate',sim,'pl.jerzy.bassambient.mobile')
    run('xcrun','simctl','launch',sim,'pl.jerzy.bassambient.mobile')
    time.sleep(3)
    run('xcrun','simctl','io',sim,'screenshot',str(out/'iPhone14-relaunch.png'))
    print('PASS iPhone 14 simulator: touch MIDI audio, menus, session/scenes, WAV/MIDI and relaunch')
finally:
    if sim:
        try:run('xcrun','simctl','shutdown',sim,timeout=30)
        except Exception as error:print(error)
