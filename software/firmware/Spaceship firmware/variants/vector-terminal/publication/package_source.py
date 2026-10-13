"""Export a clean, buildable v2 source kit; optionally verify an isolated copy.

Only allowlisted support files and variant sources are exported. Existing
working-v1 archives, compiler installs and generated test binaries stay local.
Run from the project root:
    python variants/vector-terminal/publication/package_source.py --check
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import uuid
import zipfile

HERE = Path(__file__).resolve().parent
VARIANT = HERE.parent
ROOT = VARIANT.parents[1]
DIST = VARIANT / 'dist'
NAME = 'vector-terminal-v2-source'


def digest(data):
    return hashlib.sha256(data).hexdigest()


def collect():
    """Use exact bytes, preserving inherited encodings and attribution."""
    names = [
        'build.bat', 'verify.bat', 'setup-env.bat',
        'tools/build.py', 'tools/setup.py', 'tools/verify.py',
        'tools/smoke8080.c', 'tools/toolchain.json',
        'docs/BUILD.md', 'docs/HARDWARE.md',
        'emulator/i8080.c', 'emulator/i8080.h', 'emulator/LICENSE',
        'variants/vector-terminal/build.json',
        'variants/vector-terminal/BASE.json',
        'variants/vector-terminal/SOURCE.md',
        'variants/vector-terminal/.gitignore',
        'variants/vector-terminal/tests/machine.py',
        'variants/vector-terminal/tests/cpu_bridge.c',
        'variants/vector-terminal/tests/startup_pause.py',
        'variants/vector-terminal/publication/package_source.py',
        'variants/vector-terminal/publication/accepted-v2.json',
        'variants/vector-terminal/publication/README.template.md',
    ]
    # Keep legacy source data as part of the local snapshot, even when it is
    # not selected by build.json. Never export compiler output from src.
    names.extend(str(p.relative_to(ROOT)).replace('\\', '/')
                 for p in sorted((VARIANT / 'src').rglob('*'))
                 if p.is_file() and (p.suffix in ('.c', '.h', '.asm')
                                     or p.name == '.gitignore'))
    files = {name: (ROOT / name).read_bytes() for name in names}
    files['README.md'] = (HERE / 'README.template.md').read_bytes()
    files['.gitignore'] = (
        '.tools/\nbuild/\n__pycache__/\n*.pyc\n'
        'variants/vector-terminal/dist/\n'
        'variants/vector-terminal/tests/out/\n'
    ).encode('ascii')
    files['variants/vector-terminal/README.md'] = (
        '# Vector-terminal v2\n\n'
        'See the [source guide](SOURCE.md) and the '
        '[source-kit README](../../README.md) for build and verification.\n'
    ).encode('ascii')
    for name in ('screens.png', 'anti-flicker.gif', 'anti-flicker.json',
                 'budget.json', 'startup-pause.json'):
        source = VARIANT / 'tests/out' / name
        if not source.exists():
            source = VARIANT / 'evidence' / name
        files['variants/vector-terminal/evidence/' + name] = source.read_bytes()
    # The extraction directory is not in Git. Generated paths and absolute
    # compiler locations do not belong in the source manifest.
    manifest = {
        'version': NAME,
        'expected_rom': json.loads((HERE / 'accepted-v2.json').read_text()),
        'files': {name: digest(data) for name, data in sorted(files.items())},
    }
    files['SOURCE-MANIFEST.json'] = (json.dumps(manifest, indent=2) + '\n').encode()
    return files, manifest


def export(files):
    DIST.mkdir(exist_ok=True)
    path = DIST / (NAME + '.zip')
    temporary = path.with_suffix('.zip.tmp')
    with zipfile.ZipFile(temporary, 'w', compression=zipfile.ZIP_DEFLATED,
                         compresslevel=9) as archive:
        for name, data in sorted(files.items()):
            info = zipfile.ZipInfo(NAME + '/' + name, (2026, 10, 13, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            archive.writestr(info, data)
    # Replace only this derived publication ZIP. Never touch working-v1.
    temporary.replace(path)
    return path


def check(path, files, manifest):
    """Rebuild from a fresh extraction using the installed project compiler."""
    extraction = DIST / ('check-' + uuid.uuid4().hex[:12])
    extraction.mkdir()
    with zipfile.ZipFile(path) as archive:
        expected = {NAME + '/' + name for name in files}
        assert set(archive.namelist()) == expected
        for name, data in files.items():
            assert archive.read(NAME + '/' + name) == data, name
        # The member list is generated above; still validate resolved paths
        # before extraction so this checker cannot unpack outside its folder.
        for name in archive.namelist():
            assert (extraction / name).resolve().is_relative_to(extraction.resolve())
        archive.extractall(extraction)
    checkout = extraction / NAME
    for name, sha in manifest['files'].items():
        assert digest((checkout / name).read_bytes()) == sha, name
    # Validate relative Markdown links, excluding external and generated files.
    for name in ('README.md', 'variants/vector-terminal/README.md',
                 'variants/vector-terminal/SOURCE.md'):
        p = checkout / name
        for link in re.findall(r'\]\(([^)]+)\)', p.read_text(encoding='utf-8')):
            if ':' not in link and not link.startswith('#'):
                assert (p.parent / link.split('#')[0]).exists(), (name, link)
    # No duplicate SDK installation/download for this local audit. Use the
    # current SDK and target environment, but the EXTRACTED build.py and
    # EXTRACTED source tree. A fresh user uses setup-env.bat normally.
    code = """
import importlib.util, sys
from pathlib import Path
root, checkout = map(Path, sys.argv[1:])
sys.path.insert(0, str(root / 'tools'))
import setup
env = setup.environment()
spec = importlib.util.spec_from_file_location('kit_build', checkout / 'tools/build.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
module.ROOT = checkout
for target in ('hardware', 'emulator'):
    module.build('vector-terminal', target, env)
"""
    subprocess.run([sys.executable, '-c', code, str(ROOT), str(checkout)],
                   cwd=checkout, check=True)
    results = {}
    for target, image in (('hardware', 'rom.bin'), ('emulator', 'emulator.bin')):
        folder = checkout / 'build/vector-terminal' / target
        data = (folder / image).read_bytes()
        accepted = manifest['expected_rom'][target]
        assert len(data) == manifest['expected_rom']['image_bytes']
        assert digest(data) == accepted['sha256'], target + ' ROM differs'
        report = json.loads((folder / 'build.json').read_text())
        assert report['rom_used'] == accepted['rom_used']
        results[target] = {'sha256': digest(data), 'image_bytes': len(data),
                           'rom_used': report['rom_used']}
    subprocess.run([sys.executable, str(checkout / 'tools/verify.py'),
                    'vector-terminal'], cwd=checkout, check=True)
    subprocess.run([sys.executable, str(checkout /
                    'variants/vector-terminal/tests/startup_pause.py')],
                   cwd=checkout, check=True)
    result = {'result': 'PASS', 'zip_sha256': digest(path.read_bytes()),
              'files': len(files), 'extracted_files_match_manifest': True,
              'both_ROMs_match_accepted_v2': True, 'verify': 'PASS',
              'BME_stage_test': 'PASS', 'images': results}
    (DIST / 'source-check.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    files, manifest = collect()
    path = export(files)
    if args.check:
        check(path, files, manifest)
    print('Source kit: ' + str(path))


if __name__ == '__main__':
    main()
