#!/usr/bin/env python3
"""Verify a previously converted data cache against readonly source recordings."""
import argparse,hashlib,json
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('events',type=Path);a=p.parse_args()
def sha(path):
    h=hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
    return h.hexdigest()
for recording in ['10','11']:
    manifest=json.loads((a.events/(recording+'.manifest.json')).read_text())
    if sha(a.events/(recording+'.csv'))!=manifest['csv_sha256']:raise ValueError('CSV changed: '+recording)
    if sha(a.source/(recording+'.txt'))!=manifest['source_sha256']:raise ValueError('source changed: '+recording)
    print('verified source and CSV',recording)
