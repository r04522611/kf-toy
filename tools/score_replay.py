#!/usr/bin/env python3
"""Score post-initialization GNSS antenna residual proxy (no pose truth claim)."""
import argparse,csv,hashlib,json,math
from pathlib import Path

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def score(directory):
 with (directory/'gnss.csv').open() as f:rows=list(csv.DictReader(f))
 samples=[r for r in rows if r['initialization']=='0']
 if not samples:raise ValueError('no evaluation rows')
 for r in samples:
  if r['fused']!='0':raise ValueError('post-init position fusion violates pure/heading protocol')
  if not all(math.isfinite(float(r[k])) for k in ('rx','ry','rz','pre_rx','pre_ry','pre_rz')):raise ValueError('nonfinite residual')
 rms=lambda v:math.sqrt(math.fsum(x*x for x in v)/len(v))
 x=[float(r['rx']) for r in samples];y=[float(r['ry']) for r in samples];z=[float(r['rz']) for r in samples]
 yr=[float(r['yaw_residual_rad']) for r in samples if r['heading_valid']=='1']
 init=[r for r in rows if r['initialization']=='1'][-1]
 ix,iy=float(init['rx']),float(init['ry'])
 rt=json.loads((directory/'runtime.json').read_text())
 return dict(rows=len(samples),horizontal_direct_rmse_m=rms([math.hypot(a,b) for a,b in zip(x,y)]),vertical_direct_rmse_m=rms(z),position3d_direct_rmse_m=rms([math.sqrt(a*a+b*b+c*c) for a,b,c in zip(x,y,z)]),horizontal_increment_rmse_m=rms([math.hypot(a-ix,b-iy) for a,b in zip(x,y)]),post_heading_residual_rms_deg=rms(yr)*180/math.pi if yr else None,heading_fused=rt['heading_fused'],gnss_fused=rt['gnss_fused'],gnss_sha256=sha(directory/'gnss.csv'),metric_note='GNSS antenna residual proxy; valid heading may be fused, not independent yaw truth')
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('directory',type=Path);p.add_argument('--reference',type=Path);a=p.parse_args();result=score(a.directory)
 if a.reference:
  with a.reference.open() as f:base=list(csv.DictReader(f))
  with (a.directory/'gnss.csv').open() as f:new=list(csv.DictReader(f))
  if len(base)!=len(new):raise ValueError('reference row count mismatch')
  maxdiff=0
  for old,cur in zip(base,new):
   for k in ('timestamp','initialization','heading_valid','heading_fused','fused'):
    if old[k]!=cur[k] and float(old[k])!=float(cur[k]):raise ValueError('reference event mask mismatch '+k)
   for k in ('rx','ry','rz','pre_rx','pre_ry','pre_rz'):
    maxdiff=max(maxdiff,abs(float(old[k])-float(cur[k])))
  result['reference_comparison']=dict(source=str(a.reference),sha256=sha(a.reference),maximum_residual_difference_m=maxdiff)
 text=json.dumps(result,indent=2,sort_keys=True)+'\n';(a.directory/'metrics.json').write_text(text);print(text,end='')
