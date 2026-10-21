#!/usr/bin/env python3
"""Standard-library SVG figures and the mean-metrics table for the handnote.

Usage (from the repo root, after `./build/kf_teaching results/teaching 4 12`):
    python3 docs/handnote/make_figs.py [results/teaching]
Writes docs/handnote/fig/*.svg and docs/handnote/metrics_mean.csv.
"""
import csv, html, math, sys, statistics as st
from collections import defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = Path(sys.argv[1]) if len(sys.argv) > 1 else HERE.parent.parent / 'results' / 'teaching'
OUT = HERE / 'fig'; OUT.mkdir(parents=True, exist_ok=True)
PALETTE = ['#2455a4', '#df741a', '#278566', '#a33165', '#7765b0', '#ac9027', '#459fc1', '#444444']
RECIPES = ['Step1', 'Step2', 'Step3', 'Step4', 'Step5A', 'Step5B', 'C15A', 'C15B']
BACKENDS = ['KF', 'GN', 'LM']
FONT = 'font-family="Helvetica, Arial, sans-serif"'


def read(name):
    with (ROOT / name).open() as f:
        return list(csv.DictReader(f))


def fmt(v):
    if v == 0:
        return '0'
    a = abs(v)
    if 1e-3 <= a < 1e4:
        s = ('%.4g' % v)
        return s
    return '%.0e' % v if a < 1e-3 or a >= 1e4 else '%g' % v


def nice_ticks(lo, hi, n=5):
    if hi <= lo:
        hi = lo + 1
    raw = (hi - lo) / n
    mag = 10 ** math.floor(math.log10(raw))
    r = raw / mag
    step = (1 if r < 1.5 else 2 if r < 3 else 5 if r < 7 else 10) * mag
    start = math.floor(lo / step) * step
    ticks = []
    t = start
    while t <= hi + step * 1e-9:
        if t >= lo - step * 1e-9:
            ticks.append(round(t, 12))
        t += step
    if not ticks or ticks[0] > lo:
        ticks.insert(0, round(start, 12))
    if ticks[-1] < hi:
        ticks.append(round(ticks[-1] + step, 12))
    return ticks


def log_ticks(lo, hi):
    a, b = math.floor(math.log10(lo)), math.ceil(math.log10(hi))
    if b - a > 12:
        stride = math.ceil((b - a) / 8)
    else:
        stride = 1
    return [10 ** k for k in range(a, b + 1, stride)]


def svg_header(w, h):
    return [f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}">',
            f'<rect width="{w}" height="{h}" fill="white"/>']


def line_chart(name, title, series, xlabel, ylabel, xlog=False, ylog=False, w=640, h=380,
               markers=False, bands=(), hlines=(), legend='right', dash=None):
    L, R, T, B = 70, (170 if legend == 'right' else 20), 40, 55
    pw, ph = w - L - R, h - T - B
    pts = [(x, y) for _, v in series for x, y in v
           if math.isfinite(x) and math.isfinite(y) and (not xlog or x > 0) and (not ylog or y > 0)]
    xs = [x for x, _ in pts]; ys = [y for _, y in pts]
    if ylog:
        yt = log_ticks(min(ys), max(ys)); ylo, yhi = math.log10(yt[0]), math.log10(yt[-1])
    else:
        yt = nice_ticks(min(ys + [hl for hl in hlines]), max(ys + [hl for hl in hlines])); ylo, yhi = yt[0], yt[-1]
    if xlog:
        xt = log_ticks(min(xs), max(xs)); xlo, xhi = math.log10(xt[0]), math.log10(xt[-1])
    else:
        xt = nice_ticks(min(xs), max(xs)); xlo, xhi = xt[0], xt[-1]
    tx = (lambda v: math.log10(v)) if xlog else (lambda v: v)
    ty = (lambda v: math.log10(v)) if ylog else (lambda v: v)

    def X(v): return L + pw * (tx(v) - xlo) / (xhi - xlo)
    def Y(v): return T + ph - ph * (ty(v) - ylo) / (yhi - ylo)
    s = svg_header(w, h)
    s.append(f'<text x="{L}" y="24" font-size="15" font-weight="bold" {FONT}>{html.escape(title)}</text>')
    for x0, x1 in bands:
        s.append(f'<rect x="{X(x0):.1f}" y="{T}" width="{X(x1)-X(x0):.1f}" height="{ph}" fill="#f2d7a7" opacity=".45"/>')
    for t in yt:
        y = Y(t)
        s.append(f'<line x1="{L}" y1="{y:.1f}" x2="{L+pw}" y2="{y:.1f}" stroke="#e4e4e4"/>')
        s.append(f'<text x="{L-8}" y="{y+4:.1f}" text-anchor="end" font-size="11" {FONT}>{fmt(t)}</text>')
    for t in xt:
        x = X(t)
        s.append(f'<line x1="{x:.1f}" y1="{T}" x2="{x:.1f}" y2="{T+ph}" stroke="#e4e4e4"/>')
        s.append(f'<text x="{x:.1f}" y="{T+ph+16}" text-anchor="middle" font-size="11" {FONT}>{fmt(t)}</text>')
    for hl in hlines:
        s.append(f'<line x1="{L}" y1="{Y(hl):.1f}" x2="{L+pw}" y2="{Y(hl):.1f}" stroke="#222" stroke-dasharray="5,4"/>')
    s.append(f'<path d="M{L} {T}V{T+ph}H{L+pw}" fill="none" stroke="#222"/>')
    for i, (nm, vals) in enumerate(series):
        vals = [(x, y) for x, y in vals if math.isfinite(x) and math.isfinite(y) and (not xlog or x > 0) and (not ylog or y > 0)]
        if not vals:
            continue
        col = PALETTE[i % len(PALETTE)]
        d = ' stroke-dasharray="%s"' % dash[i] if dash and dash[i] else ''
        poly = ' '.join(f'{X(x):.1f},{Y(y):.1f}' for x, y in vals)
        s.append(f'<polyline points="{poly}" fill="none" stroke="{col}" stroke-width="1.8"{d}/>')
        if markers:
            for x, y in vals:
                s.append(f'<circle cx="{X(x):.1f}" cy="{Y(y):.1f}" r="3" fill="{col}"/>')
        if legend == 'right':
            ly = T + 10 + 18 * i
            s.append(f'<line x1="{L+pw+14}" y1="{ly}" x2="{L+pw+38}" y2="{ly}" stroke="{col}" stroke-width="2.5"{d}/>')
            s.append(f'<text x="{L+pw+44}" y="{ly+4}" font-size="12" {FONT}>{html.escape(nm)}</text>')
    s.append(f'<text x="{L+pw/2:.0f}" y="{h-12}" text-anchor="middle" font-size="12" {FONT}>{html.escape(xlabel)}</text>')
    s.append(f'<text transform="translate(16,{T+ph/2:.0f}) rotate(-90)" text-anchor="middle" font-size="12" {FONT}>{html.escape(ylabel)}</text>')
    s.append('</svg>')
    (OUT / name).write_text('\n'.join(s))


def bar_chart(name, title, categories, series, ylabel, w=640, h=360, hlines=(), ylog=False, ymin0=True):
    L, R, T, B = 70, 150, 40, 60
    pw, ph = w - L - R, h - T - B
    vals = [v for _, vs in series for v in vs if math.isfinite(v) and (not ylog or v > 0)]
    if ylog:
        yt = log_ticks(min(vals), max(vals)); ylo, yhi = math.log10(yt[0]), math.log10(yt[-1])
        ty = math.log10
    else:
        lo = 0 if ymin0 else min(vals)
        yt = nice_ticks(min(lo, min(vals)), max(vals + list(hlines))); ylo, yhi = yt[0], yt[-1]
        ty = lambda v: v
    def Y(v): return T + ph - ph * (ty(v) - ylo) / (yhi - ylo)
    s = svg_header(w, h)
    s.append(f'<text x="{L}" y="24" font-size="15" font-weight="bold" {FONT}>{html.escape(title)}</text>')
    for t in yt:
        y = Y(t)
        s.append(f'<line x1="{L}" y1="{y:.1f}" x2="{L+pw}" y2="{y:.1f}" stroke="#e4e4e4"/>')
        s.append(f'<text x="{L-8}" y="{y+4:.1f}" text-anchor="end" font-size="11" {FONT}>{fmt(t)}</text>')
    for hl in hlines:
        s.append(f'<line x1="{L}" y1="{Y(hl):.1f}" x2="{L+pw}" y2="{Y(hl):.1f}" stroke="#222" stroke-dasharray="5,4"/>')
    n, m = len(categories), len(series)
    gw = pw / n
    bw = gw * .8 / m
    base = Y(yt[0])
    for i, (nm, vs) in enumerate(series):
        col = PALETTE[i % len(PALETTE)]
        for j, v in enumerate(vs):
            if not math.isfinite(v) or (ylog and v <= 0):
                continue
            x = L + j * gw + gw * .1 + i * bw
            y = Y(v)
            s.append(f'<rect x="{x:.1f}" y="{min(y,base):.1f}" width="{bw:.1f}" height="{abs(base-y):.1f}" fill="{col}"/>')
        ly = T + 10 + 18 * i
        s.append(f'<rect x="{L+pw+14}" y="{ly-6}" width="20" height="12" fill="{col}"/>')
        s.append(f'<text x="{L+pw+40}" y="{ly+4}" font-size="12" {FONT}>{html.escape(nm)}</text>')
    for j, c in enumerate(categories):
        x = L + j * gw + gw / 2
        lines = c.split('\n')
        for k, ln in enumerate(lines):
            s.append(f'<text x="{x:.1f}" y="{T+ph+16+13*k}" text-anchor="middle" font-size="11" {FONT}>{html.escape(ln)}</text>')
    s.append(f'<path d="M{L} {T}V{T+ph}H{L+pw}" fill="none" stroke="#222"/>')
    s.append(f'<text transform="translate(16,{T+ph/2:.0f}) rotate(-90)" text-anchor="middle" font-size="12" {FONT}>{html.escape(ylabel)}</text>')
    s.append('</svg>')
    (OUT / name).write_text('\n'.join(s))


F = float
# ---------- scalar ----------
sc = read('scalar.csv')
line_chart('scalar_est.svg', 'Scalar random-walk KF: estimate vs truth',
           [('truth', [(F(r['step']), F(r['truth'])) for r in sc]),
            ('measurement', [(F(r['step']), F(r['measurement'])) for r in sc]),
            ('estimate', [(F(r['step']), F(r['estimate'])) for r in sc])], 'step', 'x')
line_chart('scalar_gain.svg', 'Scalar KF: gain and posterior variance',
           [('gain K', [(F(r['step']), F(r['gain'])) for r in sc]),
            ('variance P+', [(F(r['step']), F(r['variance'])) for r in sc])], 'step', 'value', markers=True)

# ---------- integration ----------
ig = read('integration.csv')
def sel(case, samp, rec):
    return sorted(((F(r['dt']), max(F(r['velocity_error']), 1e-17)) for r in ig
                   if r['input_case'] == case and r['sampling'] == samp and r['recipe'] == rec))
line_chart('integration_held.svg', 'Held body force: velocity error vs step size (1 s, 2 rad/s)',
           [('Step1 (Euler)', sel('held_force', 'left', 'Step1')),
            ('Step2 (exact ZOH)', sel('held_force', 'left', 'Step2'))],
           'step size h (s)', '|v error| (m/s)', xlog=True, ylog=True, markers=True)
line_chart('integration_varying.svg', 'Varying body force 3cos(1.7t): velocity error vs step size',
           [('Step1, left sample', sel('varying_force', 'left', 'Step1')),
            ('Step1, midpoint sample', sel('varying_force', 'midpoint', 'Step1')),
            ('Step2, left sample', sel('varying_force', 'left', 'Step2')),
            ('Step2, midpoint sample', sel('varying_force', 'midpoint', 'Step2'))],
           'step size h (s)', '|v error| (m/s)', xlog=True, ylog=True, markers=True,
           dash=[None, '6,3', None, '6,3'])

# ---------- so3 ----------
so = read('so3.csv')
line_chart('so3.svg', 'Additive SO(3) approximations vs rotation angle',
           [('|(I+[phi]x)^T(I+[phi]x) - I|', [(F(r['angle']), F(r['first_order_orthogonality'])) for r in so]),
            ('|I - 1/2[phi]x - J_r(phi)|', [(F(r['angle']), F(r['first_order_reset_error'])) for r in so]),
            ('|exp^T exp - I|', [(F(r['angle']), max(F(r['exp_orthogonality']), 1e-17)) for r in so])],
           'angle |phi| (rad)', 'error', xlog=True, ylog=True, markers=True)

# ---------- traces ----------
tr = read('trace.csv')
def trace(scn, rec, field, backend='KF'):  # noqa
    return [(F(r['t']), F(r[field])) for r in tr if r['scenario'] == scn and r['recipe'] == rec and r['backend'] == backend]
for scn, title in [('matched_heading', 'matched_heading: KF position error, seed 0'),
                   ('pure_dr', 'pure_dr: KF position error, seed 0'),
                   ('bias_mismatch', 'bias_mismatch: KF position error, seed 0')]:
    line_chart(f'trace_{scn}_pos.svg', title, [(r, trace(scn, r, 'position_error')) for r in RECIPES], 'time (s)', 'position error (m)')
line_chart('trace_heading_dropout_att.svg', 'heading_dropout: KF attitude error, seed 0 (shaded = no heading)',
           [(r, trace('heading_dropout', r, 'attitude_error')) for r in RECIPES], 'time (s)', 'attitude error (rad)', bands=[(4.2, 9.0)])
line_chart('trace_bias_mismatch_nees.svg', 'bias_mismatch: KF NEES15/15, seed 0 (step at t = 6 s)',
           [(r, [(t, max(v / 15, 1e-3)) for t, v in trace('bias_mismatch', r, 'nees15')]) for r in RECIPES],
           'time (s)', 'NEES15 / 15', ylog=True, hlines=[1.0], bands=[(6.0, 12.0)])
line_chart('trace_bias_mismatch_bg.svg', 'bias_mismatch: KF gyro-bias error norm, seed 0',
           [(r, trace('bias_mismatch', r, 'bg_error')) for r in RECIPES], 'time (s)', '|bg error| (rad/s)', bands=[(6.0, 12.0)])
line_chart('trace_pure_dr_att.svg', 'pure_dr: KF attitude error, seed 0',
           [(r, trace('pure_dr', r, 'attitude_error')) for r in RECIPES], 'time (s)', 'attitude error (rad)')

# ---------- metrics bars ----------
me = read('metrics.csv')
agg = defaultdict(list)
for r in me:
    agg[(r['scenario'], r['recipe'], r['backend'])].append(r)
def mean(scn, rec, b, k):
    return st.mean(F(x[k]) for x in agg[(scn, rec, b)])
with (HERE / 'metrics_mean.csv').open('w', newline='') as f:
    wr = csv.writer(f)
    wr.writerow(['scenario', 'recipe', 'backend', 'pos RMSE (m)', 'att RMSE (rad)', 'NEES15/15', 'vel NIS'])
    for scn in ['matched_heading', 'heading_dropout', 'pure_dr', 'bias_mismatch']:
        for r in RECIPES:
            for b in BACKENDS:
                wr.writerow([scn, r, b, '%.4f' % mean(scn, r, b, 'position_rmse'), '%.5f' % mean(scn, r, b, 'attitude_rmse'),
                             '%.3f' % mean(scn, r, b, 'normalized_nees15'), '%.3f' % mean(scn, r, b, 'mean_velocity_nis')])
for scn in ['matched_heading', 'heading_dropout', 'pure_dr', 'bias_mismatch']:
    bar_chart(f'bars_{scn}_rmse.svg', f'{scn}: position RMSE, mean of 4 seeds', RECIPES,
              [(b, [mean(scn, r, b, 'position_rmse') for r in RECIPES]) for b in BACKENDS], 'position RMSE (m)')
    bar_chart(f'bars_{scn}_nees.svg', f'{scn}: normalized NEES15, mean of 4 seeds', RECIPES,
              [(b, [mean(scn, r, b, 'normalized_nees15') for r in RECIPES]) for b in BACKENDS], 'NEES15 / 15', hlines=[1.0],
              ylog=(scn == 'bias_mismatch'))

# ---------- constraints ----------
co = read('constraints.csv')
line_chart('constraints.svg', 'Virtual bias after a finite correction (dp_x, dbg_y = 0.2)',
           [('TG finite retraction', [(F(r['delta_p_x']), F(r['tg_finite_bnu_z'])) for r in co]),
            ('reduced (C15) retraction', [(F(r['delta_p_x']), F(r['reduced_bnu_z'])) for r in co]),
            ('prediction -1/2 dp x dbg', [(F(r['delta_p_x']), F(r['subspace_prediction'])) for r in co])],
           'position correction dp_x (m)', 'b_nu,z (m/s)', markers=True, dash=[None, None, '6,3'])

# ---------- remainder ----------
rm = [r for r in read('remainder.csv') if r['direction'] == '1']
line_chart('remainder.svg', 'Noiseless body-velocity output remainder vs perturbation scale',
           [('ordinary (A)', [(F(r['scale']), F(r['ordinary_remainder'])) for r in rm]),
            ('midpoint / C-star (B)', [(F(r['scale']), F(r['midpoint_remainder'])) for r in rm])],
           'perturbation scale', 'remainder norm', xlog=True, ylog=True, markers=True)

# ---------- anchor ----------
an = read('anchor.csv')
aagg = defaultdict(list)
for r in an:
    aagg[(r['scenario'], r['method'])].append(r)
methods = ['TG_no_anchor', 'TG_soft_anchor', 'C15_structural']
bar_chart('anchor_rmse.svg', 'Anchor study: position RMSE, mean of 4 seeds (Step5B / C15B, KF)', methods,
          [(s, [st.mean(F(x['position_rmse']) for x in aagg[(s, m)]) for m in methods]) for s in ['matched_heading', 'pure_dr']],
          'position RMSE (m)')
bar_chart('anchor_nees.svg', 'Anchor study: normalized NEES15, mean of 4 seeds', methods,
          [(s, [st.mean(F(x['normalized_nees15']) for x in aagg[(s, m)]) for m in methods]) for s in ['matched_heading', 'pure_dr']],
          'NEES15 / 15', hlines=[1.0])
line_chart('anchor_bnu.svg', 'Anchor study: final |b_nu| per seed (pure_dr)',
           [(m, [(F(x['seed']), max(F(x['final_bnu_norm']), 1e-6)) for x in aagg[('pure_dr', m)]]) for m in methods],
           'seed', '|b_nu| (m/s), floor 1e-6', ylog=True, markers=True)

# ---------- recovery ----------
rc = read('recovery.csv')
ridx = {(r['recipe'], r['backend']): r for r in rc}
bar_chart('recovery.svg', 'Recovery from 1.2 rad yaw error with one body-velocity update (truth yaw = 0)', RECIPES,
          [(b, [F(ridx[(r, b)]['posterior_yaw']) for r in RECIPES]) for b in BACKENDS], 'posterior yaw (rad)', hlines=[0.0], ymin0=False)

# ---------- assumptions ----------
asu = read('assumptions.csv')
aidx = {(r['case'], r['recipe'], r['backend']): r for r in asu}
cases = ['correct_likelihood', 'wrong_heading', 'overconfident_wrong_prior']
bar_chart('assumptions.svg', 'One heading update from prior yaw 1.2 rad: |posterior yaw - truth| (Step2)', cases,
          [(b, [F(aidx[(c, 'Step2', b)]['absolute_truth_error']) for c in cases]) for b in BACKENDS], '|yaw error| (rad)')
bar_chart('assumptions_cost.svg', 'Same cases: GN final cost (log)', cases,
          [('initial cost', [F(aidx[(c, 'Step2', 'GN')]['initial_cost']) for c in cases]),
           ('final cost', [F(aidx[(c, 'Step2', 'GN')]['final_cost']) for c in cases])], 'cost', ylog=True)

# ---------- slip ----------
sl = read('slip.csv')
sagg = defaultdict(list)
for r in sl:
    sagg[(r['case'], r['backend'], r['velocity_model'])].append(r)
cats = ['no_slip\nmeasured 3D', 'no_slip\nforward+NHC', 'side_slip\nmeasured 3D', 'side_slip\nforward+NHC']
keys = [('no_slip', 'measured_3D_velocity'), ('no_slip', 'forward_plus_NHC'), ('side_slip', 'measured_3D_velocity'), ('side_slip', 'forward_plus_NHC')]
for field, nm, fn in [('position_rmse', 'position RMSE (m)', 'slip_rmse.svg'),
                      ('lateral_95_coverage', 'lateral-velocity 95% coverage', 'slip_cov.svg'),
                      ('mean_velocity_nis', 'mean velocity NIS (3 dof)', 'slip_nis.svg')]:
    bar_chart(fn, f'Step2, lateral body velocity 0 or 0.6 m/s: {nm}', cats,
              [(b, [st.mean(F(x[field]) for x in sagg[(c, b, m)]) for c, m in keys]) for b in BACKENDS], nm,
              hlines=[0.95] if 'coverage' in field else ([3.0] if 'nis' in field else ()),
              ylog=(field == 'position_rmse'))

# ---------- chapter-restricted figures ----------
def traces(name, title, scn, field, recs, ylabel, **kw):
    line_chart(name, title, [(r, trace(scn, r, field)) for r in recs], 'time (s)', ylabel, **kw)
# ESKF chapter: Step1 only
traces('c1_matched_pos.svg', 'matched_heading: Step1 KF position error, seed 0', 'matched_heading', 'position_error', ['Step1'], 'position error (m)')
line_chart('c1_matched_nees.svg', 'matched_heading: Step1 KF NEES15/15, seed 0', [('Step1', [(t, v / 15) for t, v in trace('matched_heading', 'Step1', 'nees15')])], 'time (s)', 'NEES15 / 15', hlines=[1.0])
# Invariant chapter: Step1-4, KF
R4 = ['Step1', 'Step2', 'Step3', 'Step4']
traces('c3_pure_dr_pos.svg', 'pure_dr: KF position error, seed 0', 'pure_dr', 'position_error', R4, 'position error (m)')
traces('c3_pure_dr_att.svg', 'pure_dr: KF attitude error, seed 0', 'pure_dr', 'attitude_error', R4, 'attitude error (rad)')
traces('c3_dropout_att.svg', 'heading_dropout: KF attitude error, seed 0 (shaded = no heading)', 'heading_dropout', 'attitude_error', R4, 'attitude error (rad)', bands=[(4.2, 9.0)])
bar_chart('c3_pure_dr_bars.svg', 'pure_dr, KF, mean of 4 seeds', R4,
          [('position RMSE (m)', [mean('pure_dr', r, 'KF', 'position_rmse') for r in R4]),
           ('NEES15 / 15', [mean('pure_dr', r, 'KF', 'normalized_nees15') for r in R4])], 'value', hlines=[1.0])
# TG chapter: Step3, Step4, Step5A, C15A, KF
R5 = ['Step3', 'Step4', 'Step5A', 'C15A']
bar_chart('c4_pure_dr_bars.svg', 'pure_dr, KF, mean of 4 seeds', R5,
          [('position RMSE (m)', [mean('pure_dr', r, 'KF', 'position_rmse') for r in R5]),
           ('NEES15 / 15', [mean('pure_dr', r, 'KF', 'normalized_nees15') for r in R5])], 'value', hlines=[1.0])
# C-star chapter: KF only, A vs B
RAB = ['Step5A', 'Step5B', 'C15A', 'C15B']
bar_chart('c5_recovery_kf.svg', 'One body-velocity update from 1.2 rad yaw error, KF (truth yaw = 0)', RAB,
          [('posterior yaw (rad)', [F(ridx[(r, 'KF')]['posterior_yaw']) for r in RAB])], 'posterior yaw (rad)', hlines=[0.0], ymin0=False)
bar_chart('c5_pure_dr_bars.svg', 'pure_dr, KF, mean of 4 seeds: output A vs B', RAB,
          [('position RMSE (m)', [mean('pure_dr', r, 'KF', 'position_rmse') for r in RAB]),
           ('NEES15 / 15', [mean('pure_dr', r, 'KF', 'normalized_nees15') for r in RAB])], 'value', hlines=[1.0])
# GN/LM chapter: full recovery + all recipes/backends pure_dr (existing recovery.svg, bars_pure_dr_*)
# ---------- sanity: Galilean matrix exponential closed form ----------
def matmul(A, B):
    return [[sum(A[i][k] * B[k][j] for k in range(len(B))) for j in range(len(B[0]))] for i in range(len(A))]
def hat(x):
    return [[0, -x[2], x[1]], [x[2], 0, -x[0]], [-x[1], x[0], 0]]
w, a, h = [0.3, -0.2, 0.5], [1.0, 2.0, 9.8], 0.4
W = [[*hat(w)[i], a[i], 0] for i in range(3)] + [[0, 0, 0, 0, 1], [0, 0, 0, 0, 0]]
Wh = [[h * v for v in row] for row in W]
E = [[1.0 if i == j else 0.0 for j in range(5)] for i in range(5)]
term = [row[:] for row in E]
for k in range(1, 40):
    term = matmul(term, Wh); term = [[v / k for v in row] for row in term]
    E = [[E[i][j] + term[i][j] for j in range(5)] for i in range(5)]
A = hat([h * v for v in w]); G1 = [[1.0 if i == j else 0.0 for j in range(3)] for i in range(3)]; G2 = [[0.5 if i == j else 0.0 for j in range(3)] for i in range(3)]
P = [[1.0 if i == j else 0.0 for j in range(3)] for i in range(3)]
for k in range(1, 40):
    P = matmul(P, A)
    G1 = [[G1[i][j] + P[i][j] / math.factorial(k + 1) for j in range(3)] for i in range(3)]
    G2 = [[G2[i][j] + P[i][j] / math.factorial(k + 2) for j in range(3)] for i in range(3)]
col2 = [h * sum(G1[i][j] * a[j] for j in range(3)) for i in range(3)]
col3 = [h * h * sum(G2[i][j] * a[j] for j in range(3)) for i in range(3)]
err = max(abs(E[i][3] - col2[i]) for i in range(3)) + max(abs(E[i][4] - col3[i]) for i in range(3)) + abs(E[3][4] - h)
assert err < 1e-12, err
print('Galilean exponential closed form verified, err=%.1e' % err)
print('wrote', len(list(OUT.glob('*.svg'))), 'figures to', OUT)
