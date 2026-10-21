#!/usr/bin/env python3
"""Summarize sealed validation without modifying any per-run deterministic files."""
import argparse,hashlib,json,math
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--matrix',type=Path,default=Path('results/research'));p.add_argument('--output',type=Path,default=Path('results/validation.md'));p.add_argument('--tolerance',type=float,default=.001);a=p.parse_args()
m=json.loads((a.matrix/'matrix.json').read_text());rows=m['rows']
if m['completed_runs']!=96 or m['expected_runs']!=96 or m['failures']:raise RuntimeError('full96 matrix has not passed')
if not all(r['deterministic_verified'] for r in rows):raise RuntimeError('not all cells byte-verified')
refs=[r for r in rows if r['reference_available']]
maximum=max(max(abs(r['reference_horizontal_rmse_difference_m']),abs(r['reference_position_rmse_difference_m'])) for r in refs)
if maximum>a.tolerance:raise RuntimeError('reference tolerance exceeded')
clock=0
for r in rows:
    root=a.matrix/r['scenario']/r['data']/r['method'].replace(' ','_')
    seal=json.loads((root/'complete.json').read_text())
    for name,h in seal['outputs'].items():
        if hashlib.sha256((root/name).read_bytes()).hexdigest()!=h:raise RuntimeError('sealed output changed '+str(root/name))
    rt=json.loads((root/'runtime.json').read_text());clock=max(clock,abs(rt['clock_error_s']))
    if rt['gnss_fused']!=0:raise RuntimeError('post-init position fusion')
summary=dict(runs=len(rows),reference_cells=len(refs),new_c15_iterative_cells=len(rows)-len(refs),byte_repeat_verified=len(rows),max_archive_rmse_difference_m=maximum,archive_tolerance_m=a.tolerance,max_clock_error_s=clock)
a.output.parent.mkdir(parents=True,exist_ok=True)
a.output.with_suffix('.json').write_text(json.dumps(summary,indent=2,sort_keys=True)+'\n')
text='# Standalone KF validation\n\n'
text+=f"- Full matrix: **{len(rows)}/96 passed**; each run byte-identical on repeat.\n- Archived references: **{len(refs)} cells**, maximum horizontal/3D RMSE discrepancy **{maximum:.9g} m** (declared tolerance {a.tolerance:g} m).\n- New C15 iterative cases: **{len(rows)-len(refs)}**, no invented archived baseline.\n- Maximum runner clock error: **{clock:.9g} s**. No post-initialization GNSS position fusion.\n\n"
text+='Road scores are direct GNSS antenna residual proxies, not independent SE3 truth. Bitwise repeat applies to this image/binary/input/protocol, not arbitrary platforms. Archive compatibility and uniform numerical policies are recorded per run.\n\n'
text+='|Mode|Data|Method|Horizontal RMSE (m)|3D RMSE (m)|Archive delta horizontal (m)|\n|---|---|---|---:|---:|---:|\n'
for r in rows:
    delta=r['reference_horizontal_rmse_difference_m'];d='new' if delta is None else f'{delta:.9g}'
    text+=f"|{r['scenario']}|{r['data']}|{r['method']}|{r['horizontal_direct_rmse_m']:.6f}|{r['position3d_direct_rmse_m']:.6f}|{d}|\n"
a.output.write_text(text)
print(json.dumps(summary,indent=2))
