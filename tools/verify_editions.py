#!/usr/bin/env python3
"""Compare all shared tracked assets across English and Mandarin checkouts."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def shared_files(root):
    raw = subprocess.check_output(
        ['git', '-C', str(root), 'ls-files', '-z', '--cached', '--others', '--exclude-standard'])
    names = {name.decode('utf-8') for name in raw.split(b'\0') if name}
    return {name: hashlib.sha256((root / name).read_bytes()).hexdigest()
            for name in names
            if not name.startswith('docs/') and not name.startswith('README')
            and (root / name).is_file()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--peer', type=Path, required=True, help='other edition checkout')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    peer = args.peer.resolve()
    left, right = shared_files(root), shared_files(peer)
    missing_here = sorted(set(right) - set(left))
    missing_there = sorted(set(left) - set(right))
    changed = sorted(name for name in left.keys() & right.keys() if left[name] != right[name])
    digest = hashlib.sha256(json.dumps(left, sort_keys=True).encode()).hexdigest()
    result = dict(shared_files=len(left), shared_sha256=digest,
                  missing_here=missing_here, missing_in_peer=missing_there, different=changed)
    print(json.dumps(result, indent=2, sort_keys=True))
    return int(bool(missing_here or missing_there or changed))


if __name__ == '__main__':
    raise SystemExit(main())
