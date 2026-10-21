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
def render(chinese=False):
    stem=a.output.stem
    nav=f'[繁體中文]({stem}.zh-TW.md) | [English]({stem}.en.md)\n\n'
    if chinese:
        text='# 獨立 KF 驗證總報告\n\n'+nav
        text+=f"- 執行與確定性：**{len(rows)}/96 通過**；每組重跑輸出逐 byte 相同。\n- 歷史參考：**{len(refs)} 組**；水平／3D RMSE 最大差異 **{maximum:.9g} m**（原宣告容差 {a.tolerance:g} m）。\n- 新增 C15 迭代案例：**{len(rows)-len(refs)} 組**，沒有捏造歷史基準。\n- runner 最大時鐘誤差：**{clock:.9g} s**；初始化後沒有融合 GNSS position。\n\n"
        text+=f"**嚴格歷史容差檢查：** {len(refs)-len(reference_failures)}/{len(refs)} 在 {a.tolerance:g} m 內；{len(reference_failures)} 組超出。此項與執行／確定性分開判定，不能宣稱全部通過。\n\n"
    else:
        text='# Standalone KF validation\n\n'+nav
        text+=f"- Execution/determinism: **{len(rows)}/96 passed**; each run byte-identical on repeat.\n- Archived references: **{len(refs)} cells**, maximum horizontal/3D RMSE discrepancy **{maximum:.9g} m** (declared tolerance {a.tolerance:g} m).\n- New C15 iterative cases: **{len(rows)-len(refs)}**, no invented archived baseline.\n- Maximum runner clock error: **{clock:.9g} s**. No post-initialization GNSS position fusion.\n\n"
        text+=f"**Strict archive tolerance:** {len(refs)-len(reference_failures)}/{len(refs)} within {a.tolerance:g} m; {len(reference_failures)} outside. This gate is separate from deterministic execution and is not reported as fully passed.\n\n"
    for failure in reference_failures:
        label=('水平差異','3D 差異') if chinese else ('horizontal delta','3D delta')
        text+=f"- {failure['data']} / {failure['mode']} / {failure['method']}: {label[0]} {failure['horizontal_difference_m']:.9g} m, {label[1]} {failure['position3d_difference_m']:.9g} m.\n"
    if chinese:
        text+='\n真實道路分數是直接 GNSS antenna residual proxy，並非獨立 SE3 真值。逐 byte 重跑只適用於相同 image／binary／input／protocol，不保證任意平台一致。每組 run 記錄 archive compatibility 或 uniform 數值策略。\n\n'
        text+='以下為完整96組結果；pure／heading 與 recipe／backend 名稱保留機器識別字，方便對照原始 CSV。「new」表示沒有歷史參考，不代表誤差為零。\n\n'
        text+='|模式|資料|方法|水平 RMSE (m)|3D RMSE (m)|歷史水平差異 (m)|\n|---|---|---|---:|---:|---:|\n'
    else:
        text+='\nRoad scores are direct GNSS antenna residual proxies, not independent SE3 truth. Bitwise repeat applies to this image/binary/input/protocol, not arbitrary platforms. Archive compatibility and uniform numerical policies are recorded per run.\n\n'
        text+='All 96 cells follow. Mode, recipe and backend identifiers match the source CSV. "new" means no archived reference, not zero error.\n\n'
        text+='|Mode|Data|Method|Horizontal RMSE (m)|3D RMSE (m)|Archive delta horizontal (m)|\n|---|---|---|---:|---:|---:|\n'
    for r in rows:
        delta=r['reference_horizontal_rmse_difference_m'];d='new' if delta is None else f'{delta:.9g}'
        text+=f"|{r['scenario']}|{r['data']}|{r['method']}|{r['horizontal_direct_rmse_m']:.6f}|{r['position3d_direct_rmse_m']:.6f}|{d}|\n"
    return text

english=render();chinese=render(True)
a.output.write_text(english,encoding='utf-8')
a.output.with_name(a.output.stem+'.en.md').write_text(english,encoding='utf-8')
a.output.with_name(a.output.stem+'.zh-TW.md').write_text(chinese,encoding='utf-8')
print(json.dumps(summary,indent=2))

if a.require_reference and reference_failures:raise SystemExit('Strict archive comparison failed; see explicit failure list in report.')
