#!/usr/bin/env python3
"""Content-address source/config/input/environment without nondeterministic timestamps."""
import argparse,hashlib,json,pathlib,subprocess
p=argparse.ArgumentParser();p.add_argument('--root',type=pathlib.Path,default=pathlib.Path('.'));p.add_argument('--output',type=pathlib.Path,required=True);p.add_argument('--image-id',default='unknown');p.add_argument('--inputs',type=pathlib.Path,nargs='*',default=[]);a=p.parse_args()
r=a.root.resolve();sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
files={}
for prefix in ['include','src','tests','examples','configs','tools','vendor','cmake','docs','references']:
    for f in sorted((r/prefix).rglob('*')):
        if f.is_file() and '__pycache__' not in f.parts:files[str(f.relative_to(r))]=sha(f)
for n in ['CMakeLists.txt','Dockerfile']:
    files[n]=sha(r/n)
try:rev=subprocess.check_output(['git','-C',str(r),'rev-parse','HEAD'],stderr=subprocess.DEVNULL,text=True).strip()
except subprocess.CalledProcessError:rev='uncommitted-initial-tree'
manifest={'schema':1,'source_revision':rev,'source_sha256':files,'source_tree_sha256':hashlib.sha256(json.dumps(files,sort_keys=True).encode()).hexdigest(),'image_id':a.image_id,'inputs':{str(f):sha(f) for f in a.inputs}}
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(manifest,indent=2,sort_keys=True)+'\n')
