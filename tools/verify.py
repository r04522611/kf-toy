import csv, hashlib, json, math, pathlib, subprocess, sys
exe, destination = sys.argv[1:]
p = pathlib.Path(destination); p.mkdir(parents=True, exist_ok=True)
fixture=p/'input.csv'
x=y=yaw=0.0
with fixture.open('w', newline='') as f:
    w=csv.writer(f,lineterminator='\n');w.writerow(['t_s','gyro_z_rad_s','speed_m_s','heading_rad','truth_x_m','truth_y_m','truth_yaw_rad'])
    for i in range(6001):
        t=i*0.01
        w.writerow([format(t,'.17g'),0.025,2,format(yaw,'.17g') if i%100==0 and not 2000<=i<4000 else '',format(x,'.17g'),format(y,'.17g'),format(yaw,'.17g')])
        rate=0.02;dt=0.01
        x+=2/rate*(math.sin(yaw+rate*dt)-math.sin(yaw))
        y+=2/rate*(math.cos(yaw)-math.cos(yaw+rate*dt));yaw+=rate*dt
truth=list(csv.DictReader(fixture.open()))
metrics={}
for mode in ['estimate_bias','fixed_bias']:
    a=p/(mode+'.csv');b=p/(mode+'.repeat.csv')
    for target in [a,b]:subprocess.run([exe,str(fixture),str(target),mode],check=True)
    if a.read_bytes()!=b.read_bytes():raise RuntimeError('non-deterministic replay')
    output=list(csv.DictReader(a.open()))
    rmse=math.sqrt(sum((float(q['x_m'])-float(t['truth_x_m']))**2+(float(q['y_m'])-float(t['truth_y_m']))**2 for q,t in zip(output,truth))/len(truth))
    metrics[mode]={'position_rmse_m':rmse,'final_bias_rad_s':float(output[-1]['bias_rad_s']),'sha256':hashlib.sha256(a.read_bytes()).hexdigest()}
if metrics['estimate_bias']['position_rmse_m']>=metrics['fixed_bias']['position_rmse_m']:raise RuntimeError('bias estimation regression')
if abs(metrics['estimate_bias']['final_bias_rad_s']-0.005)>0.0002:raise RuntimeError('bias recovery regression')
# Reject malformed data and non-monotonic clocks.
for name,content in [('bad','bad header\n'),('clock',fixture.read_text().splitlines()[0]+'\n0,0,0,,0,0,0\n0,0,0,,0,0,0\n')]:
    q=p/(name+'.csv');q.write_text(content)
    if subprocess.run([exe,str(q),str(p/'invalid.csv'),'estimate_bias'],capture_output=True).returncode==0:raise RuntimeError('invalid input accepted')
(p/'metrics.json').write_text(json.dumps(metrics,indent=2)+'\n')
print(json.dumps(metrics,indent=2))

manifest={'input_sha256':hashlib.sha256(fixture.read_bytes()).hexdigest(),'binary_sha256':hashlib.sha256(pathlib.Path(exe).read_bytes()).hexdigest(),'script_sha256':hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),'fixture':'analytic-circle-60s-v1','input_hold':'previous row ZOH','heading_dropout_s':[20,40]}
(p/'run-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
report='# Planar KF toy comparison\n\nSame binary replayed twice per method: byte-identical.\n\n|Method|Position RMSE (m)|Final gyro bias (rad/s)|\n|---|---:|---:|\n'
for mode,m in metrics.items():report+=f"|{mode}|{m['position_rmse_m']:.6f}|{m['final_bias_rad_s']:.8f}|\n"
(p/'comparison.md').write_text(report)
