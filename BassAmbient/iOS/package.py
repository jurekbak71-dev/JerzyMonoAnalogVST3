"""Package a device .app with its executable permissions; Sideloadly signs locally."""
from pathlib import Path
import plistlib
import subprocess
import sys
import zipfile

app=Path(sys.argv[1]);out=Path(sys.argv[2]);out.mkdir(parents=True,exist_ok=True)
with (app/'Info.plist').open('rb') as f: info=plistlib.load(f)
assert info['CFBundleIdentifier']=='pl.jerzy.bassambient.mobile'
assert info['CFBundleSupportedPlatforms']==['iPhoneOS'], 'Never package a simulator build as a device IPA'
exe=app/info['CFBundleExecutable']
assert exe.stat().st_mode & 0o111, 'Executable permissions must survive packaging'
assert 'arm64' in subprocess.check_output(['lipo','-archs',str(exe)],text=True)
name=f"Bass-Ambient-{info['CFBundleShortVersionString']}-iOS.ipa"
with zipfile.ZipFile(out/name,'w',zipfile.ZIP_DEFLATED) as z:
    for p in sorted(app.rglob('*')):
        if p.is_file():z.write(p,str(Path('Payload')/app.name/p.relative_to(app)))
with zipfile.ZipFile(out/name) as z: assert z.testzip() is None
print(out/name)
