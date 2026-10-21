#!/usr/bin/env python3
"""Summarize sealed validation without modifying any per-run deterministic files."""
import argparse,hashlib,json,math
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--matrix',type=Path,default=Path('results/research'));p.add_argument('--output',type=Path,default=Path('results/validation.md'));p.add_argument('--tolerance',type=float,default=.001);p.add_argument('--require-reference',action='store_true',help='also fail on archived numeric tolerance, separate from execution/determinism');a=p.parse_args()
m=json.loads((a.matrix/'matrix.json').read_text());rows=m['rows']
if m['completed_runs']!=96 or m['expected_runs']!=96 or m['failures']:raise RuntimeError('full96 matrix has not passed')
if not all(r['deterministic_verified'] for r in rows):raise RuntimeError('not all cells byte-verified')
refs=[r for r in rows if r['reference_available']]
maximum=max(max(abs(r['reference_horizontal_rmse_difference_m']),abs(r['reference_position_rmse_difference_m'])) for r in refs)
reference_failures=[{'method':r['method'],'data':r['data'],'mode':r['scenario'],'horizontal_difference_m':r['reference_horizontal_rmse_difference_m'],'position3d_difference_m':r['reference_position_rmse_difference_m']} for r in refs if max(abs(r['reference_horizontal_rmse_difference_m']),abs(r['reference_position_rmse_difference_m']))>a.tolerance]
clock=0
for r in rows:
    root=a.matrix/r['scenario']/r['data']/r['method'].replace(' ','_')
    seal=json.loads((root/'complete.json').read_text())
    for name,h in seal['outputs'].items():
        if hashlib.sha256((root/name).read_bytes()).hexdigest()!=h:raise RuntimeError('sealed output changed '+str(root/name))
    rt=json.loads((root/'runtime.json').read_text());clock=max(clock,abs(rt['clock_error_s']))
    if rt['gnss_fused']!=0:raise RuntimeError('post-init position fusion')
summary=dict(runs=len(rows),reference_cells=len(refs),new_c15_iterative_cells=len(rows)-len(refs),byte_repeat_verified=len(rows),max_archive_rmse_difference_m=maximum,archive_tolerance_m=a.tolerance,max_clock_error_s=clock,archive_strict_pass=not reference_failures,archive_failures=reference_failures,archive_cells_within_tolerance=len(refs)-len(reference_failures))
a.output.parent.mkdir(parents=True,exist_ok=True)
a.output.with_suffix('.json').write_text(json.dumps(summary,indent=2,sort_keys=True)+'\n')
text='# Standalone KF validation\n\n'
text+=f"- Execution/determinism: **{len(rows)}/96 passed**; each run byte-identical on repeat.\n- Archived references: **{len(refs)} cells**, maximum horizontal/3D RMSE discrepancy **{maximum:.9g} m** (declared tolerance {a.tolerance:g} m).\n- New C15 iterative cases: **{len(rows)-len(refs)}**, no invented archived baseline.\n- Maximum runner clock error: **{clock:.9g} s**. No post-initialization GNSS position fusion.\n\n"
text+=f"**Strict archive tolerance:** {len(refs)-len(reference_failures)}/{len(refs)} within {a.tolerance:g} m; {len(reference_failures)} outside. This gate is separate from deterministic execution and is not reported as fully passed.\n\n"
for failure in reference_failures:
    text+=f"- {failure['data']} / {failure['mode']} / {failure['method']}: horizontal delta {failure['horizontal_difference_m']:.9g} m, 3D delta {failure['position3d_difference_m']:.9g} m.\n"
text+='\nRoad scores are direct GNSS antenna residual proxies, not independent SE3 truth. Bitwise repeat applies to this image/binary/input/protocol, not arbitrary platforms. Archive compatibility and uniform numerical policies are recorded per run.\n\n'
text+='|Mode|Data|Method|Horizontal RMSE (m)|3D RMSE (m)|Archive delta horizontal (m)|\n|---|---|---|---:|---:|---:|\n'
for r in rows:
    delta=r['reference_horizontal_rmse_difference_m'];d='new' if delta is None else f'{delta:.9g}'
    text+=f"|{r['scenario']}|{r['data']}|{r['method']}|{r['horizontal_direct_rmse_m']:.6f}|{r['position3d_direct_rmse_m']:.6f}|{d}|\n"
a.output.write_text(text)
print(json.dumps(summary,indent=2))

if a.require_reference and reference_failures:raise SystemExit('Strict archive comparison failed; see explicit failure list in report.')
