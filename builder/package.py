"""Package an actual native launcher + bundled Python/Tk and ROM template."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--template', type=Path, default=ROOT / 'gbavocab.gba')
    parser.add_argument('--output', type=Path, default=ROOT / 'builder-dist')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    template = ROOT / 'builder/template.gba'
    if args.template.resolve() != template.resolve():
        shutil.copy2(args.template, template)
    # uv standalone Python keeps Tcl/Tk 9 shared libraries outside the
    # dynamic loader's normal search path; include those actual dependencies.
    extra = []
    if sys.platform.startswith('linux'):
        for library in (Path(sys.base_prefix) / 'lib').glob('libtcl*.so'):
            extra.extend(['--add-binary', str(library) + os.pathsep + '.'])
    subprocess.run([sys.executable, '-m', 'PyInstaller', *extra, '--noconfirm', '--clean', '--windowed', '--onedir', '--name', 'gbavocab-builder', '--distpath', str(args.output), '--workpath', str(args.output / 'work'), '--specpath', str(args.output), '--add-data', str(template) + os.pathsep + '.', str(ROOT / 'builder/app.py')], check=True)
    folder = args.output / 'gbavocab-builder'
    shutil.copy2(ROOT / 'LICENSE', folder / 'LICENSE.txt')
    shutil.copy2(ROOT / 'docs/full-controls.md', folder / 'INSTRUCTIONS.md')
    exe = folder / ('gbavocab-builder.exe' if sys.platform == 'win32' else 'gbavocab-builder')
    if not exe.is_file():
        raise RuntimeError('Packaged executable missing')
    if sys.platform == 'win32' and exe.read_bytes()[:2] != b'MZ':
        raise RuntimeError('Not a Windows PE executable')
    qa = args.output / 'verification'
    subprocess.run([str(exe), '--self-test', str(qa)], check=True, timeout=120)
    report = json.loads((qa / 'self-test.json').read_text())
    assert report['roundtrip'] and report['packaged'] and report['entries'] == 40011
    subprocess.run([str(exe), '--gui-smoke', str(qa)], check=True, timeout=60)
    smoke = json.loads((qa / 'gui-smoke.json').read_text())
    assert smoke['window_created'] and smoke['packaged']
    archive = shutil.make_archive(str(args.output / ('gbavocab-builder-windows' if sys.platform == 'win32' else 'gbavocab-builder-linux')), 'zip', args.output, folder.name)
    manifest = dict(executable=str(exe.relative_to(args.output)), executable_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(), archive=Path(archive).name, archive_sha256=hashlib.sha256(Path(archive).read_bytes()).hexdigest(), self_test=report, gui_smoke=smoke)
    (qa / 'package.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
    print(json.dumps(manifest, indent=2))

if __name__ == '__main__':
    main()
