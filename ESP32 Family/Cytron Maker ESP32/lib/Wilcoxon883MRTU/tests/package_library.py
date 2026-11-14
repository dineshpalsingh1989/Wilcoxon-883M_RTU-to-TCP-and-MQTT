"""Create the Arduino library ZIP and checksum from an allowlist."""
import argparse
import hashlib
import json
from pathlib import Path
import re
from zipfile import ZipFile, ZIP_DEFLATED


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--tag', help='Require the tag to match the beta manifest version')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    manifest = json.loads((root / 'library.json').read_text(encoding='utf-8'))
    version = manifest['version']
    properties = dict(line.split('=', 1) for line in
                      (root / 'library.properties').read_text(encoding='utf-8').splitlines() if '=' in line)
    if not re.fullmatch(r'\d+\.\d+\.\d+-beta\.\d+', version):
        parser.error('The library release must have an explicit beta version')
    if properties.get('version') != version or properties.get('name') != manifest['name']:
        parser.error('Arduino and PlatformIO manifests disagree')
    header = (root / 'src' / 'Wilcoxon883MVersion.h').read_text(encoding='utf-8')
    if '#define WILCOXON883M_LIBRARY_VERSION "' + version + '"' not in header:
        parser.error('src/Wilcoxon883MVersion.h does not match the manifest version')
    if args.tag and args.tag != 'wilcoxon883mrtu-v' + version:
        parser.error('Release tag does not match the beta library version')
    args.output.mkdir(parents=True, exist_ok=True)
    output = args.output / ('Wilcoxon883MRTU-' + version + '.zip')
    files = [root / name for name in ['library.json', 'library.properties', 'LICENSE', 'README.md', 'CHANGELOG.md', 'RELEASE_NOTES.md']]
    files += sorted(p for p in (root / 'src').rglob('*') if p.is_file())
    files += sorted(p for p in (root / 'examples').rglob('*.ino') if p.is_file())
    with ZipFile(output, 'w', ZIP_DEFLATED) as archive:
        for path in files:
            archive.write(path, (Path('Wilcoxon883MRTU') / path.relative_to(root)).as_posix())
    with ZipFile(output) as archive:
        if archive.testzip() is not None:
            raise RuntimeError('ZIP verification failed')
    checksum = hashlib.sha256(output.read_bytes()).hexdigest()
    (args.output / 'SHA256SUMS.txt').write_text(checksum + '  ' + output.name + '\n', encoding='utf-8')
    print(output)


if __name__ == '__main__':
    main()
