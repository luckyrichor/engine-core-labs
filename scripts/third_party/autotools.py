"""Optional Ubuntu build tools, extracted into ECL_DATA without sudo/dpkg install."""
import hashlib
import json
import os
from pathlib import Path
import subprocess

PACKAGES = ('autoconf=2.71-3', 'automake=1:1.16.5-1.3ubuntu1',
            'libtool=2.4.7-7build1', 'm4=1.4.19-4build1', 'autotools-dev=20220109.1')


def prepare(data):
    base = data/'build-tools'
    root = base/'root'
    marker = base/'receipt.json'
    if not marker.exists() or json.loads(marker.read_text())['packages'] != list(PACKAGES):
        packages = base/'packages'
        packages.mkdir(parents=True, exist_ok=True)
        subprocess.run(['apt-get', 'download', *PACKAGES], cwd=packages, check=True)
        for package in sorted(packages.glob('*.deb')):
            subprocess.run(['dpkg-deb', '-x', str(package), str(root)], check=True)
        # Debian maintainer scripts normally create these alternatives. No
        # maintainer script is executed by this user-local extraction.
        for name in ('automake', 'aclocal'):
            link = root/'usr/bin'/name
            if not link.exists():
                link.symlink_to(name+'-1.16')
        # Relocate generated scripts/config modules. Leave binaries and frozen
        # macro state unchanged. This affects only the extracted tool copies.
        for folder in (root/'usr/bin', root/'usr/share'):
            for path in folder.rglob('*'):
                if not path.is_file() or path.is_symlink() or path.suffix == '.m4f':
                    continue
                try:
                    text = path.read_text()
                except UnicodeDecodeError:
                    continue
                changed = text.replace('/usr/share/', str(root/'usr/share')+'/')
                for name in ('autoconf', 'autoheader', 'autom4te', 'autoreconf', 'm4'):
                    changed = changed.replace('/usr/bin/'+name, str(root/'usr/bin'/name))
                if changed != text:
                    path.write_text(changed)
        marker.write_text(json.dumps({'packages': list(PACKAGES), 'root': str(root),
            'sha256': {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                       for p in sorted(packages.glob('*.deb'))}}, indent=2)+'\n')
    os.environ['PATH'] = str(root/'usr/bin')+os.pathsep+os.environ['PATH']
    return json.loads(marker.read_text())
