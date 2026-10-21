#!/usr/bin/env python3
"""Synthetic integration checks: protocol information must not leak into update."""
import csv,json,subprocess,sys,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from convert_events import convert
BINARY=Path(sys.argv[1]).resolve() if len(sys.argv)>1 else ROOT/'build/replay3d'
if len(sys.argv)>1:sys.argv=sys.argv[:1]
class ReplayProtocol(unittest.TestCase):
 def fixture(self,p,evaluation_lat='39.9'):
  lines=[]
  for k in range(105):
   t=k/10
   lines.append((t,0,f'ODOM {t:.1f} 0 0'))
   lines.append((t,1,f'IMU {t:.1f} 0 0 0 0 0 9.81'))
  lines +=[(10.5,0,'GNSS 10.5 39.9 116.4 30 77.94 1'),(10.6,0,'GNSS 10.6 39.9 116.4 30 77.94 1'),(10.7,0,f'GNSS 10.7 {evaluation_lat} 116.4 30 80 1'),(10.8,0,f'GNSS 10.8 {evaluation_lat} 116.4 30 85 0')]
  source=p/'input.txt';source.write_text('\n'.join(x[2] for x in sorted(lines))+'\n');convert(source,p/'events.csv','synthetic');return p/'events.csv'
 def run_replay(self,inp,out,mode='pure',backend='kf'):
  return subprocess.run([str(BINARY),'--input',str(inp),'--output',str(out),'--recipe','step2','--backend',backend,'--mode',mode],capture_output=True,text=True)
 def test_initialization_and_no_evaluation_position_leak(self):
  with tempfile.TemporaryDirectory() as td:
   p=Path(td);(p/'one').mkdir();(p/'two').mkdir();a=self.fixture(p/'one');b=self.fixture(p/'two','40.9')
   for inp,name in [(a,'baseline'),(b,'shifted')]:
    result=self.run_replay(inp,p/name);self.assertEqual(result.returncode,0,result.stderr)
   self.assertEqual((p/'baseline/states.txt').read_bytes(),(p/'shifted/states.txt').read_bytes())
   self.assertEqual((p/'baseline/covariance.csv').read_bytes(),(p/'shifted/covariance.csv').read_bytes())
   self.assertNotEqual((p/'baseline/gnss.csv').read_bytes(),(p/'shifted/gnss.csv').read_bytes())
   rt=json.loads((p/'baseline/runtime.json').read_text());self.assertEqual(rt['gnss_fused'],0);self.assertEqual(rt['heading_fused'],0);self.assertEqual(rt['heldout'],2);self.assertLess(abs(rt['clock_error_s']),1e-10)
   init=json.loads((p/'baseline/initialization.json').read_text());self.assertEqual(init['first_fix_time'],10.5);self.assertEqual(init['evaluation_start'],10.6)
 def test_heading_validity_mask_and_iterative_backend(self):
  with tempfile.TemporaryDirectory() as td:
   p=Path(td);inp=self.fixture(p)
   for backend in ['kf','gn','lm']:
    result=self.run_replay(inp,p/backend,'heading',backend);self.assertEqual(result.returncode,0,result.stderr)
    rt=json.loads((p/backend/'runtime.json').read_text());self.assertEqual(rt['heading_fused'],1);self.assertEqual(rt['gnss_fused'],0)
    with (p/backend/'gnss.csv').open() as f:rows=list(csv.DictReader(f))
    self.assertEqual([r['heading_fused'] for r in rows if r['initialization']=='0'],['1','0'])
 def test_parser_rejects_mixed_recordings_and_payload(self):
  with tempfile.TemporaryDirectory() as td:
   p=Path(td);inp=self.fixture(p);original=inp.read_text()
   lines=original.splitlines();tokens=lines[-1].split(',');tokens[0]='different';lines[-1]=','.join(tokens);inp.write_text('\n'.join(lines)+'\n')
   result=self.run_replay(inp,p/'mixed');self.assertNotEqual(result.returncode,0);self.assertIn('multiple recording',result.stderr)
   lines=original.splitlines();index=next(i for i,line in enumerate(lines) if ',IMU,' in line);tokens=lines[index].split(',');tokens[4]='nan';lines[index]=','.join(tokens);inp.write_text('\n'.join(lines)+'\n')
   result=self.run_replay(inp,p/'nonfinite');self.assertNotEqual(result.returncode,0);self.assertIn('finite',result.stderr)
if __name__=='__main__':unittest.main()
