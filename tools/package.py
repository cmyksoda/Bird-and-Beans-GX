#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Package the Wii app and license notices for installation on an SD card."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import xml.etree.ElementTree as ET
import zipfile

ROOT = Path(__file__).resolve().parent.parent


def package(build, out):
    voices = ROOT / 'assets/voices.pak'
    data = voices.read_bytes()
    manifest = json.loads((ROOT / 'assets/voices-manifest.json').read_text())
    count = sum(entry['include'] for entry in manifest['clips'])
    if (data[:8] != b'BBV1' + struct.pack('<I', count)
            or hashlib.sha256(data).hexdigest() != manifest['pack_sha256']):
        raise ValueError('Vendored voices.pak does not match its manifest.')

    meta = ROOT / 'assets/meta.xml'
    ET.parse(meta)
    notices = [ROOT / 'LICENSE', *sorted((ROOT / 'licenses').glob('*.txt'))]
    out.mkdir(parents=True, exist_ok=True)
    target = out / 'Bird-and-Beans-GX.zip'
    with zipfile.ZipFile(target, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        archive.write(ROOT / 'README.md', 'README.md')
        for path in notices:
            archive.write(path, path.relative_to(ROOT))
        archive.write(build / 'boot.dol', 'apps/birdbeans/boot.dol')
        archive.write(voices, 'apps/birdbeans/voices.pak')
        archive.write(ROOT / 'assets/icon.png', 'apps/birdbeans/icon.png')
        archive.write(meta, 'apps/birdbeans/meta.xml')
        archive.writestr(
            'INSTALL.txt',
            '1. Copy apps/ to the root of your SD card.\n'
            '2. Put your USA .nds ROM in apps/birdbeans/, beside boot.dol. Any filename is fine.\n'
            '3. Launch Bird & Beans GX in the Homebrew Channel.\n'
            'Game data is prepared automatically on first launch; unzip the ROM first if needed.\n'
            'Transition voices are included. Keep scores.dat when updating.\n'
            'Source code and build tools are available in the project repository.\n')
    (out / 'SHA256SUMS').write_text(
        hashlib.sha256(target.read_bytes()).hexdigest() + '  ' + target.name + '\n')
    print(target, target.stat().st_size)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=Path.home() / '.cache/birdbeans-build')
    parser.add_argument('--output', type=Path, default=ROOT / 'dist')
    args = parser.parse_args()
    package(args.build, args.output)
