#!/usr/bin/env python3
"""Resumable independent replay jobs and optional deterministic byte verification."""
import argparse,concurrent.futures,csv,hashlib,json,shutil,subprocess,sys
from pathlib import Path
from score_replay import score,sha
RECIPES=['step1','step2','step3','step4','step5a','step5b','c15a','c15b']
BACKENDS=['kf','gn','lm']
def main():
 p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,default=Path('build/replay3d'));p.add_argument('--data',type=Path,default=Path('data/events'));p.add_argument('--output',type=Path,default=Path('results/matrix'));p.add_argument('--jobs',type=int,default=1);p.add_argument('--recipes',nargs='+',choices=RECIPES,default=RECIPES);p.add_argument('--backends',nargs='+',choices=BACKENDS,default=BACKENDS);p.add_argument('--datasets',nargs='+',default=['10','11']);p.add_argument('--modes',nargs='+',choices=['pure','heading'],default=['pure','heading']);p.add_argument('--repeat',action='store_true');p.add_argument('--reference',type=Path,default=Path('references/archived-metrics.json'));p.add_argument('--protocol',choices=['archive','uniform'],default='archive');p.add_argument('--reference-tolerance',type=float,default=None,help='fail if archived RMSE difference exceeds this many metres');a=p.parse_args()
 if a.jobs<1:p.error('jobs must be positive')
 a.binary=a.binary.resolve();a.output.mkdir(parents=True,exist_ok=True)
 binary_hash=sha(a.binary)
 script_hashes={f.name:sha(f) for f in [Path(__file__),Path(__file__).with_name('score_replay.py')]}
 archived=json.loads(a.reference.read_text())['rows'] if a.reference.exists() else []
 jobs=[(d,m,r,b) for d in a.datasets for m in a.modes for r in a.recipes for b in a.backends]
 input_hashes={d:sha(a.data/f'{d}.csv') for d in a.datasets}
 def run(job):
  d,m,r,b=job;dest=a.output/m/d/f'{r}_{b}';dest.parent.mkdir(parents=True,exist_ok=True)
  signature=dict(protocol=a.protocol,reference_sha256=sha(a.reference) if a.reference.exists() else None,tool_sha256=script_hashes,binary_sha256=binary_hash,input_sha256=input_hashes[d],dataset=d,mode=m,recipe=r,backend=b)
  stamp=dest/'complete.json'
  if dest.exists():
   if not stamp.exists() or json.loads(stamp.read_text()).get('signature')!=signature:raise ValueError(f'incomplete or changed run at {dest}; choose a new output directory')
   sealed=json.loads(stamp.read_text())['outputs']
   if any(not (dest/f).exists() or sha(dest/f)!=h for f,h in sealed.items()):raise ValueError(f'changed sealed outputs at {dest}')
  else:
   cmd=[str(a.binary),'--input',str(a.data/f'{d}.csv'),'--output',str(dest),'--recipe',r,'--backend',b,'--mode',m,'--protocol',a.protocol]
   result=subprocess.run(cmd,text=True,capture_output=True)
   if result.returncode:raise RuntimeError(f'{dest}: {result.stderr}')
   (dest/'metrics.json').write_text(json.dumps(score(dest),indent=2,sort_keys=True)+'\n')
   outputs={f.name:sha(f) for f in sorted(dest.iterdir()) if f.is_file()}
   stamp.write_text(json.dumps(dict(signature=signature,outputs=outputs,deterministic_verified=False),indent=2,sort_keys=True)+'\n')
  if a.repeat and not json.loads(stamp.read_text()).get('deterministic_verified',False):
   repeat=dest.with_name(dest.name+'-repeat')
   if repeat.exists():raise ValueError(f'repeat directory already exists: {repeat}')
   cmd=[str(a.binary),'--input',str(a.data/f'{d}.csv'),'--output',str(repeat),'--recipe',r,'--backend',b,'--mode',m,'--protocol',a.protocol]
   subprocess.run(cmd,check=True,capture_output=True,text=True)
   for f in sorted(repeat.iterdir()):
    if sha(f)!=sha(dest/f.name):raise ValueError(f'non-deterministic output {f.name}: {job}')
   shutil.rmtree(repeat)
   seal=json.loads(stamp.read_text());seal['deterministic_verified']=True;stamp.write_text(json.dumps(seal,indent=2,sort_keys=True)+'\n')
  result=dict(data=d,scenario=m,method=f'{r} {b}',**json.loads((dest/'metrics.json').read_text()))
  ref=next((x for x in archived if str(x['recording'])==d and x['mode']==m and x['method']==f'{r}_{b}'),None)
  result['reference_available']=ref is not None
  result['reference_horizontal_rmse_difference_m']=result['horizontal_direct_rmse_m']-ref['horizontal_rmse_m'] if ref else None
  result['reference_position_rmse_difference_m']=result['position3d_direct_rmse_m']-ref['position_rmse_m'] if ref else None
  if ref and a.reference_tolerance is not None and max(abs(result['reference_horizontal_rmse_difference_m']),abs(result['reference_position_rmse_difference_m']))>a.reference_tolerance:raise ValueError(f'archived metric tolerance exceeded: {job}')
  result['reference_rows_match']=result['rows']==ref['rows'] if ref else None
  result['deterministic_verified']=json.loads(stamp.read_text()).get('deterministic_verified',False)
  if ref and (result['rows']!=ref['rows'] or result['heading_fused']!=ref['heading_fused']):raise ValueError(f'reference evaluation mask/count mismatch: {job}')
  return result
 selection=dict(protocol=a.protocol,datasets=a.datasets,modes=a.modes,recipes=a.recipes,backends=a.backends)
 selection_file=a.output/'selection.json'
 if selection_file.exists() and json.loads(selection_file.read_text())!=selection:raise ValueError('matrix selection changed; use new output directory')
 selection_file.write_text(json.dumps(selection,indent=2,sort_keys=True)+'\n')
 rows=[];failures=[]
 with concurrent.futures.ThreadPoolExecutor(max_workers=a.jobs) as ex:
  futures={ex.submit(run,j):j for j in jobs}
  for f in concurrent.futures.as_completed(futures):
   try:rows.append(f.result());print('completed',futures[f],flush=True)
   except Exception as e:failures.append(dict(job=futures[f],error=str(e)));print(str(e),file=sys.stderr,flush=True)
 rows.sort(key=lambda x:(x['scenario'],x['data'],x['method']))
 (a.output/'matrix.json').write_text(json.dumps(dict(expected_runs=len(jobs),completed_runs=len(rows),deterministic_repeat=a.repeat,failures=failures,rows=rows),indent=2,sort_keys=True)+'\n')
 if rows:
  with (a.output/'comparison.csv').open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
 return bool(failures)
if __name__=='__main__':sys.exit(main())
