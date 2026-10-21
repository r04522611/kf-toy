#!/usr/bin/env python3
"""Dependency-free static SVG plots for reproducible scientific teaching outputs."""
import argparse,csv,html,math
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('directory',type=Path);a=p.parse_args();root=a.directory
palette=['#2455a4','#df741a','#278566','#a33165','#7765b0','#ac9027','#459fc1','#444444']
def svg_plot(path,title,series,xlabel,ylabel,log=False):
    pts=[(x,y) for _,v in series for x,y in v if math.isfinite(x) and math.isfinite(y) and (not log or (x>0 and y>0))]
    if not pts:return
    trans=lambda v:math.log10(v) if log else v
    xs=[trans(x) for x,y in pts];ys=[trans(y) for x,y in pts]
    xmin,xmax=min(xs),max(xs);ymin,ymax=min(ys),max(ys)
    if xmax==xmin:xmax=xmin+1
    if ymax==ymin:ymax=ymin+1
    def xy(x,y):return 90+650*(trans(x)-xmin)/(xmax-xmin),430-330*(trans(y)-ymin)/(ymax-ymin)
    s=['<svg xmlns="http://www.w3.org/2000/svg" width="1100" height="520" viewBox="0 0 1100 520">','<rect width="1100" height="520" fill="white"/>',f'<text x="90" y="35" font-size="22">{html.escape(title)}</text>','<path d="M90 85V430H750" fill="none" stroke="#222"/>']
    for i in range(6):
        xx=90+650*i/5;yy=430-330*i/5
        vx=xmin+(xmax-xmin)*i/5;vy=ymin+(ymax-ymin)*i/5
        if log:vx=10**vx;vy=10**vy
        s += [f'<text x="{xx}" y="455" text-anchor="middle" font-size="12">{vx:.3g}</text>',f'<text x="80" y="{yy}" text-anchor="end" font-size="12">{vy:.3g}</text>']
    for i,(name,values) in enumerate(series):
        values=[(x,y) for x,y in values if math.isfinite(x) and math.isfinite(y) and (not log or (x>0 and y>0))]
        points=' '.join('%.3f,%.3f'%xy(x,y) for x,y in values)
        color=palette[i%len(palette)]
        s += [f'<polyline points="{points}" fill="none" stroke="{color}" stroke-width="1.8"/>',f'<text x="780" y="{95+22*i}" fill="{color}" font-size="13">{html.escape(name)}</text>']
    s += [f'<text x="390" y="495" text-anchor="middle" font-size="15">{html.escape(xlabel)}</text>',f'<text x="15" y="65" font-size="15">{html.escape(ylabel)}</text>','</svg>']
    path.write_text('\n'.join(s))
rows=list(csv.DictReader((root/'trace.csv').open()))
for scenario in sorted({r['scenario'] for r in rows}):
    series=[]
    for recipe in ['Step1','Step2','Step3','Step4','Step5A','Step5B','C15A','C15B']:
        r=[r for r in rows if r['scenario']==scenario and r['recipe']==recipe and r['backend']=='KF']
        series.append((recipe,[(float(q['t']),float(q['position_error'])) for q in r]))
    svg_plot(root/(scenario+'.svg'),scenario+' — KF position error',series,'Time (s)','Position error (m)')
r=list(csv.DictReader((root/'remainder.csv').open()));r=[q for q in r if q['direction']=='1']
svg_plot(root/'remainder.svg','Noiseless output remainder (log-log)',[(n,[(float(q['scale']),float(q[k])) for q in r]) for n,k in [('Ordinary','ordinary_remainder'),('Midpoint','midpoint_remainder')]],'Perturbation scale','Remainder',True)
print('Wrote SVG plots to',root)
