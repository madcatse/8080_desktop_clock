"""Verify a built ROM on the bundled 8080 core (requires native Clang)."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
from build import ROOT, symbols

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('variant', nargs='?', default='original')
args = parser.parse_args()
clang = shutil.which('clang') or r'C:\Program Files\LLVM\bin\clang.exe'
out = ROOT / 'build/verification'
out.mkdir(parents=True, exist_ok=True)
exe = out / 'smoke8080.exe'
subprocess.run([clang, '-O2', '-std=c99', '-D_CRT_SECURE_NO_WARNINGS', '-Wno-unused-command-line-argument',
                '-I' + str(ROOT / 'emulator'), str(ROOT / 'tools/smoke8080.c'),
                str(ROOT / 'emulator/i8080.c'), '-o', str(exe)], check=True)
for target in ('hardware', 'emulator'):
    folder = ROOT / 'build' / args.variant / target
    report = json.loads((folder / 'build.json').read_text())
    stage = ROOT / report['work_directory']
    sym = symbols(folder / 'clock80.map')
    image = folder / ('rom.bin' if target == 'hardware' else 'emulator.bin')
    result = subprocess.run([str(exe), str(image), str(stage / 'clock80_DATA.bin'),
                            str(sym['__DATA_head']), str(sym['_main']),
                            '0' if target == 'hardware' else '60000000',
                            str(folder / 'screen.ppm')], check=True, capture_output=True, text=True)
    (folder / 'verification.txt').write_text(result.stdout)
    print(target + ':\n' + result.stdout)
