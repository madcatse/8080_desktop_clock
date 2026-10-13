"""Install the pinned official Windows z88dk archive inside this project."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
LOCK = json.loads((ROOT / 'tools/toolchain.json').read_text())
SDK = ROOT / '.tools' / ('z88dk-' + LOCK['version']) / 'z88dk'


def install():
    if not (SDK / 'bin/zcc.exe').is_file():
        archive = ROOT / '.tools/downloads' / LOCK['url'].rsplit('/', 1)[-1]
        archive.parent.mkdir(parents=True, exist_ok=True)
        if not archive.exists():
            print('Downloading official z88dk ' + LOCK['version'], flush=True)
            partial = archive.with_suffix('.part')
            request = urllib.request.Request(LOCK['url'], headers={'User-Agent': 'clock80-build'})
            with urllib.request.urlopen(request, timeout=120) as response, partial.open('wb') as out:
                shutil.copyfileobj(response, out)
            partial.replace(archive)
        digest = hashlib.sha256(archive.read_bytes()).hexdigest()
        if digest != LOCK['sha256']:
            raise RuntimeError('Toolchain SHA256 mismatch: ' + str(archive))
        with zipfile.ZipFile(archive) as source:
            destination = SDK.parent.resolve()
            for name in source.namelist():
                if not (destination / name).resolve().is_relative_to(destination):
                    raise RuntimeError('Unsafe archive path: ' + name)
            source.extractall(destination)
        (SDK.parent / 'verified.json').write_text(json.dumps(LOCK, indent=2) + '\n')
    # Refresh only our custom target, never system-wide z88dk files or PATH.
    target = SDK / 'lib/target/clock80/classic'
    target.mkdir(parents=True, exist_ok=True)
    for source, destination in (
        (ROOT / 'z88dk_target/clock80.cfg', SDK / 'lib/config/clock80.cfg'),
        (ROOT / 'z88dk_target/clock80/classic/clock80_crt0.asm', target / 'clock80_crt0.asm'),
    ):
        if not destination.exists() or source.read_bytes() != destination.read_bytes():
            shutil.copy2(source, destination)
    return SDK


def environment():
    install()
    env = os.environ.copy()
    env['PATH'] = str(SDK / 'bin') + os.pathsep + env.get('PATH', '')
    env['ZCCCFG'] = str(SDK / 'lib/config') + os.sep
    env['Z80_OZFILES'] = str(SDK / 'lib') + os.sep
    return env


if __name__ == '__main__':
    print('Ready: ' + str(install()))
