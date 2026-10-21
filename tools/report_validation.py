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
ENGLISH = {
    'language': 'en',
    'title': 'Standalone KF validation',
    'navigation': '[English repository](https://github.com/r04522611/kf-toy) | [Traditional Chinese validation report](https://github.com/r04522611/kf-toy-mandarin/blob/codex/kf-cpp-toy/docs/validation.md)',
    'summary': '- Execution/determinism: **{runs}/96 passed**; each run byte-identical on repeat.\n- Archived references: **{refs} cells**, maximum horizontal/3D RMSE discrepancy **{maximum:.9g} m** (declared tolerance {tolerance:g} m).\n- New C15 iterative cases: **{new}**, no invented archived baseline.\n- Maximum runner clock error: **{clock:.9g} s**. No post-initialization GNSS position fusion.',
    'strict': '**Strict archive tolerance:** {within}/{refs} within {tolerance:g} m; {failures} outside. This gate is separate from deterministic execution and is not reported as fully passed.',
    'failure': '- {data} / {mode} / {method}: horizontal delta {horizontal_difference_m:.9g} m, 3D delta {position3d_difference_m:.9g} m.',
    'limitations': 'Road scores are direct GNSS antenna residual proxies, not independent SE3 truth. Bitwise repeat applies to this image/binary/input/protocol, not arbitrary platforms. Archive compatibility and uniform numerical policies are recorded per run.',
    'rows_note': 'All 96 cells follow. Mode, recipe and backend identifiers match the source CSV. "new" means no archived reference, not zero error.',
    'header': '|Mode|Data|Method|Horizontal RMSE (m)|3D RMSE (m)|Archive delta horizontal (m)|',
}
locale_path=Path(__file__).resolve().parents[1]/'docs'/'report-locale.json'
locale=dict(ENGLISH)
if locale_path.exists():
    translated=json.loads(locale_path.read_text(encoding='utf-8'))
    if set(translated)!=set(ENGLISH) or not all(isinstance(v,str) for v in translated.values()):
        raise ValueError('invalid report locale keys or value types')
    locale.update(translated)
values=dict(runs=len(rows),refs=len(refs),maximum=maximum,tolerance=a.tolerance,
            new=len(rows)-len(refs),clock=clock,within=len(refs)-len(reference_failures),failures=len(reference_failures))
parts=['# '+locale['title'],locale['navigation'],locale['summary'].format(**values),locale['strict'].format(**values)]
if reference_failures:
    parts.append('\n'.join(locale['failure'].format(**failure) for failure in reference_failures))
parts.extend([locale['limitations'],locale['rows_note']])
table=locale['header']+'\n|---|---|---|---:|---:|---:|\n'
for r in rows:
    delta=r['reference_horizontal_rmse_difference_m'];d='new' if delta is None else f'{delta:.9g}'
    table+=f"|{r['scenario']}|{r['data']}|{r['method']}|{r['horizontal_direct_rmse_m']:.6f}|{r['position3d_direct_rmse_m']:.6f}|{d}|\n"
parts.append(table.rstrip())
a.output.write_text('\n\n'.join(parts)+'\n',encoding='utf-8')
print(json.dumps(summary,indent=2))

if a.require_reference and reference_failures:raise SystemExit('Strict archive comparison failed; see explicit failure list in report.')
