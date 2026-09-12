#!/usr/bin/env python3
"""Check or apply the recorded ROM patches to an exact, clean source checkout."""
import argparse
import json
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source_root', type=Path)
    parser.add_argument('--apply', action='store_true', help='apply after all checks pass')
    args = parser.parse_args()
    here = Path(__file__).resolve().parent
    projects = json.loads((here / 'projects.json').read_text())
    pending = []
    for project in projects:
        directory = args.source_root.resolve() / project['path']
        command = ['git', '-C', str(directory)]
        head = subprocess.check_output(command + ['rev-parse', 'HEAD'], text=True).strip()
        if head != project['base']:
            raise SystemExit(f"Wrong base for {project['path']}: {head}")
        changes = subprocess.check_output(command + ['status', '--porcelain'], text=True)
        if changes:
            raise SystemExit(f"Preserve local changes before applying: {project['path']}")
        patch = here / 'patches' / project['patch']
        subprocess.run(command + ['apply', '--check', '--index', str(patch)], check=True)
        pending.append((command, patch))
        print(f"Checked {project['path']} at {head}")
    if args.apply:
        for command, patch in pending:
            subprocess.run(command + ['apply', '--index', str(patch)], check=True)
        print('Applied and staged all patches; review git diff --cached in each project.')
    else:
        print('No files changed. Pass --apply to apply the checked patches.')


if __name__ == '__main__':
    main()
