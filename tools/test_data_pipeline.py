#!/usr/bin/env python3
"""Pipeline contract tests without numpy or access to the original repository."""
import csv,json,subprocess,tempfile,unittest
from pathlib import Path
from convert_events import convert,FIELDS,SPEC
from score_replay import score
class Pipeline(unittest.TestCase):
 def test_roundtrip_tokens_order_counts(self):
  with tempfile.TemporaryDirectory() as td:
   p=Path(td);src=p/'source.txt';out=p/'events.csv'
   events=['ODOM 1.00000000000000001 0 0','IMU 1.00000000000000001 1e-10 -2.1 3 4 5 6','GNSS 2 39.9 116.4 32.3 180 1']
   src.write_text('# comment\n\n'+'\n'.join(events)+'\n');manifest=convert(src,out,'demo')
   with out.open() as f:rows=list(csv.DictReader(f))
   self.assertEqual(manifest['counts'],{'ODOM':1,'IMU':1,'GNSS':1})
   for seq,(tokens,r) in enumerate(zip(map(str.split,events),rows)):
    self.assertEqual(r['sequence'],str(seq));self.assertEqual([r['type'],r['timestamp_s']]+[r[k] for k in SPEC[r['type']]],tokens)
 def test_malformed_sources(self):
  bad=['IMU 1 nan 0 0 0 0 0','UNKNOWN 1 2','ODOM 1 0','ODOM 2 0 0\nODOM 1 0 0','GNSS 1 1 2 3 4 2']
  for text in bad:
   with self.subTest(text=text),tempfile.TemporaryDirectory() as td:
    p=Path(td);(p/'in').write_text(text)
    with self.assertRaises(ValueError):convert(p/'in',p/'out','x')
 def test_scoring_masks_and_leakage(self):
  with tempfile.TemporaryDirectory() as td:
   p=Path(td);header='timestamp,fused,heading_valid,initialization,rx,ry,rz,yaw_residual_rad,nis,heading_fused,pre_rx,pre_ry,pre_rz,pre_yaw_residual_rad\n'
   first='1,1,1,1,0,0,0,0,0,0,0,0,0,0\n';sample='2,0,1,0,3,4,12,0,0,0,3,4,12,0\n'
   (p/'gnss.csv').write_text(header+first+sample);(p/'runtime.json').write_text('{"heading_fused":0,"gnss_fused":0}')
   m=score(p);self.assertEqual(m['rows'],1);self.assertEqual(m['position3d_direct_rmse_m'],13)
   (p/'gnss.csv').write_text(header+first+sample.replace('2,0,1,0','2,1,1,0'))
   with self.assertRaises(ValueError):score(p)
 def test_cli_rejects_unknown_argument(self):
  binary=Path(__file__).resolve().parents[1]/'build'/'replay3d'
  if not binary.exists():self.skipTest('replay3d not built')
  result=subprocess.run([str(binary),'--invalid','value'],capture_output=True,text=True)
  self.assertNotEqual(result.returncode,0);self.assertIn('unknown argument',result.stderr)
if __name__=='__main__':unittest.main()
