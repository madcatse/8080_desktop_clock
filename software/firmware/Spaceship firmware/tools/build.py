"""Build isolated 8080 ROMs; never publish an oversized or stale image."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import uuid
from datetime import datetime, timezone
from setup import ROOT, SDK, LOCK, environment

SOURCES = ['main.c', 'lcd.c', 'gui.c', 'cust_fnt.c', 'i2c.c', 'onewire.c',
           'bme280.c', 'font.c', 'forecast.c']


def symbols(path):
    return {name: int(value, 16) for name, value in re.findall(
        r'^(\S+)\s*=\s*\$([0-9A-Fa-f]+)', path.read_text(), re.M)}


def build(variant, target, env):
    base = ROOT / 'firmware' if variant == 'original' else ROOT / 'variants' / variant / 'src'
    if not base.is_dir():
        raise ValueError('Source directory does not exist: ' + str(base))
    manifest = base.parent / 'build.json' if variant != 'original' else ROOT / 'tools/original.json'
    config = json.loads(manifest.read_text()) if manifest.exists() else {}
    sources = config.get('sources', SOURCES)
    if not sources:
        raise ValueError('No source files selected')
    for name in sources:
        path = (base / name).resolve()
        if not path.is_relative_to(base.resolve()) or not path.is_file():
            raise ValueError('Missing or invalid source: ' + name)

    output = ROOT / 'build' / variant / target
    output.mkdir(parents=True, exist_ok=True)
    # Remove only this target's published outputs before attempting a new build.
    for name in ('rom.bin', 'emulator.bin', 'build.json', 'clock80.map',
                 'verification.txt', 'screen.ppm', 'screen.png'):
        (output / name).unlink(missing_ok=True)
    stage = output / ('work-' + uuid.uuid4().hex[:12])
    shutil.copytree(base, stage)
    args = [str(SDK / 'bin/zcc.exe'), '+clock80', *sources,
            '-o', 'clock80.bin', '--list', '-m', '-create-app',
            '--math-dai32']
    if target == 'emulator':
        args.append('-DEMU=1')
    args.extend(config.get('options', []))
    args.extend(['-m8080', '-pragma-define:CRT_MODEL=2'])
    with (output / 'build.log').open('w', encoding='utf-8') as log:
        result = subprocess.run(args, cwd=stage, env=env, stdout=log, stderr=subprocess.STDOUT)
    print((output / 'build.log').read_text(), end='')
    if result.returncode:
        raise RuntimeError('Compiler failed; see ' + str(output / 'build.log'))
    raw = (stage / 'clock80.rom').read_bytes()
    if not 1 <= len(raw) <= 32768:
        raise ValueError(f'ROM uses {len(raw)} bytes; 32768-byte limit exceeded. No image published.')
    sym = symbols(stage / 'clock80.map')
    data_start = sym['__DATA_head']
    data_end = sym['__DATA_END_tail']
    bss_start = sym['__BSS_head']
    stack = sym['__register_sp']
    if stack != 0xFE00 or sym.get('__CPU_INTEL__') != 1:
        raise ValueError('Unexpected CPU or initial stack')
    if bss_start != 0x8000 or not 0x8000 <= data_start <= data_end < stack:
        raise ValueError('Invalid RAM layout or collision with initial stack')
    if sym['__ROMABLE_END_tail'] > 0x8000 or sym['__crt_model'] != 2:
        raise ValueError('Invalid ROM layout or startup model')
    if raw[0] != 0xF3:  # DI at reset, before jumping to startup.
        raise ValueError('Unexpected reset vector')
    image = raw + b'\xff' * (32768 - len(raw))
    name = 'rom.bin' if target == 'hardware' else 'emulator.bin'
    (output / name).write_bytes(image)
    shutil.copy2(stage / 'clock80.map', output / 'clock80.map')
    report = {
        'variant': variant, 'target': target, 'toolchain': LOCK,
        'built_at_utc': datetime.now(timezone.utc).isoformat(),
        'rom_used': len(raw), 'rom_free': 32768 - len(raw), 'image_bytes': len(image),
        'sha256': hashlib.sha256(image).hexdigest(),
        'bss_start': bss_start, 'data_start': data_start, 'data_end_exclusive': data_end,
        'initial_stack': stack, 'static_to_stack_gap': stack - data_end,
        'command': args, 'work_directory': str(stage.relative_to(ROOT)),
        'source_sha256': {str(p.relative_to(base)): hashlib.sha256(p.read_bytes()).hexdigest()
                          for p in sorted(base.rglob('*')) if p.is_file()},
        'target_sha256': {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                          for p in sorted((ROOT / 'z88dk_target').rglob('*')) if p.is_file()},
    }
    (output / 'build.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f'{variant}/{target}: {len(raw)}/32768 ROM bytes; {stack-data_end} bytes to stack')
    print('Image: ' + str(output / name))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('variant', nargs='?', default='original')
    parser.add_argument('target', nargs='?', choices=['all', 'hardware', 'emulator'], default='all')
    args = parser.parse_args()
    if not re.fullmatch(r'[a-z][a-z0-9-]*', args.variant):
        parser.error('Variant must be a lowercase identifier without path separators')
    env = environment()
    for target in (['hardware', 'emulator'] if args.target == 'all' else [args.target]):
        build(args.variant, target, env)


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, RuntimeError, KeyError) as exc:
        raise SystemExit('BUILD FAILED: ' + str(exc))
