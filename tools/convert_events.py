#!/usr/bin/env python3
"""Lossless numeric-token conversion: source order is never sorted."""
import argparse, collections, csv, hashlib, json, math
from pathlib import Path
FIELDS='recording_id sequence timestamp_s type gx_rad_s gy_rad_s gz_rad_s ax_m_s2 ay_m_s2 az_m_s2 left_pulse right_pulse latitude_deg longitude_deg altitude_m heading_deg heading_valid'.split()
SPEC={'IMU':FIELDS[4:10], 'ODOM':FIELDS[10:12], 'GNSS':FIELDS[12:]}
def sha(p):
 h=hashlib.sha256()
 with open(p,'rb') as f:
  for b in iter(lambda:f.read(1024*1024),b''): h.update(b)
 return h.hexdigest()
def convert(source,output,recording):
 counts=collections.Counter(); last=-math.inf; seq=0
 output.parent.mkdir(parents=True,exist_ok=True)
 with source.open() as src, output.open('x',newline='') as dst:
  w=csv.DictWriter(dst,fieldnames=FIELDS,lineterminator='\n');w.writeheader()
  for line_no,line in enumerate(src,1):
   s=line.strip()
   if not s or s.startswith('#'):continue
   tok=s.split();kind=tok[0]
   if kind not in SPEC or len(tok)!=len(SPEC[kind])+2:raise ValueError(f'{source}:{line_no}: invalid event')
   if not all(math.isfinite(float(x)) for x in tok[1:]):raise ValueError(f'{source}:{line_no}: nonfinite')
   t=float(tok[1])
   if t<last:raise ValueError(f'{source}:{line_no}: reversed timestamp')
   if kind=='GNSS' and tok[-1] not in ('0','1'):raise ValueError('heading_valid must be 0/1')
   row=dict(recording_id=recording,sequence=seq,timestamp_s=tok[1],type=kind)
   row.update(zip(SPEC[kind],tok[2:]));w.writerow(row);seq+=1;counts[kind]+=1;last=t
 manifest=dict(schema='kf-events-v1',recording_id=recording,source=str(source.resolve()),source_sha256=sha(source),csv_sha256=sha(output),rows=seq,counts=dict(counts),ordering='original source sequence; nondecreasing timestamp',numeric_conversion='original decimal tokens retained',timestamp_unit='seconds',gyro_unit='rad/s',acceleration_unit='m/s^2')
 output.with_suffix('.manifest.json').write_text(json.dumps(manifest,indent=2,sort_keys=True)+'\n')
 return manifest
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('output',type=Path);p.add_argument('--recording');a=p.parse_args()
 print(json.dumps(convert(a.source,a.output,a.recording or a.source.stem),sort_keys=True))
