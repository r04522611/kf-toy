#!/usr/bin/env python3
"""Seal archived *metrics*, never copy large road recordings into Git."""
import argparse,csv,hashlib,json,pathlib
p=argparse.ArgumentParser()
p.add_argument('--overview',type=pathlib.Path,required=True)
p.add_argument('--source-repo',type=pathlib.Path,required=True)
p.add_argument('--output',type=pathlib.Path,required=True)
a=p.parse_args()
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
rows=[]
for name in ['backend-results.csv','all-results.csv']:
    path=a.overview/name
    for r in csv.DictReader(path.open()):
        method=r['method'].lower().replace(' ','_')
        if '+_anchor' in method:continue
        if name=='all-results.csv' and method not in ['c15_a','c15_b']:continue
        method=method.replace('c15_a','c15a_kf').replace('c15_b','c15b_kf')
        source=a.source_repo/pathlib.Path(r['source']).relative_to('/sad')
        actual=sha(source)
        if actual!=r['gnss_sha256']:raise RuntimeError('reference changed: '+str(source))
        rows.append(dict(method=method,mode=r['scenario'],recording=r['data'],rows=int(r['rows']),
                         horizontal_rmse_m=float(r['horizontal_direct_rmse_m']),
                         position_rmse_m=float(r['position3d_direct_rmse_m']),heading_fused=int(r['heading_fused']),
                         original_source=r['source'],gnss_sha256=actual))
out={'schema':1,'notes':['Archived proxy metrics; not independent SE3 truth.','TG uses epoch-repaired outputs.','C15 GN/LM has no archived baseline; compare internal equivalence, not invented targets.'],
     'overview_sha256':sha(a.overview/'report.zh-TW.md'),'rows':rows}
a.output.parent.mkdir(parents=True,exist_ok=True)
a.output.write_text(json.dumps(out,indent=2,sort_keys=True)+'\n')
print('Verified reference cells:',len(rows))
