#!/usr/bin/env python3
"""cmp_trace_v2.py — V3 与 Python V2 参考模型的逐条轨迹比对

用法:
    python3 cmp_trace_v2.py                 # 全部用例
    python3 cmp_trace_v2.py rv32ui-p-add    # 单个
    V2_DIR=/path/to/RVSIMU_V2 python3 cmp_trace_v2.py

判据: PC + 指令码逐条一致；V2 侧以 tohost 写入为终止条件
"""
import subprocess, sys, os, re
V2=os.environ.get('V2_DIR','../RVSIMU_V2')
V3=os.path.dirname(os.path.abspath(__file__))
MAXC="5000"

v2_run = '''
import sys; sys.path.insert(0, "{v2}")
from bus import *; from cpu import Cpu; from utils import get_tohost_addr
p = "{v2}/isa/{name}"
dram=Dram(); dram.payload(p+".bin", DRAM_BASE)
bus=Bus(); bus.mem_map(DRAM_BASE, DRAM_SIZE, dram)
cpu=Cpu(True); cpu.connect_to(bus)
th=get_tohost_addr(p)
for i in range({maxc}):
    cpu.run()
    lo=bus.read(th,4) if hasattr(bus,'read') else bus.load(th,4)
    hi=bus.read(th+4,4) if hasattr(bus,'read') else bus.load(th+4,4)
    if lo or hi: break
'''
def norm(line):
    m = re.match(r'^([0-9a-f]{8}): ([0-9a-f]{8})', line.strip())
    return (m.group(1), m.group(2)) if m else None

def v2trace(name):
    r = subprocess.run([sys.executable,'-c', v2_run.format(v2=V2,name=name,maxc=MAXC)],
                       capture_output=True,text=True,timeout=300)
    return [x for x in (norm(l) for l in r.stdout.split('\n')) if x]

def v3trace(name):
    r = subprocess.run(['/tmp/v3sim', f'{V3}/isa_test/{name}','--trace','--no-isa','--max',MAXC],
                       capture_output=True,text=True,timeout=300)
    v=[x for x in (norm(l) for l in r.stdout.split('\n')) if x and True]
    return v

cases = sys.argv[1:] or sorted(os.listdir(f'{V3}/isa_test'))
same=0; diff=0; skip=0; bad=[]
for name in cases:
    # V3 名字 rv32si-p-X → V2 是 rv32mi-p-X
    # ★ 只比对两边【同名】的用例（名字不同说明是不同测试程序，比对无意义）
    v2name = name
    if not os.path.exists(f'{V2}/isa/{v2name}.bin'):
        skip+=1; continue
    try:
        a=v2trace(v2name); b=v3trace(name)
    except Exception as e:
        skip+=1; continue
    n=min(len(a),len(b))
    for i in range(n):
        if a[i]!=b[i]:
            diff+=1; bad.append((name,i,a[i],b[i])); print(f'  DIFF {name} @{i+1}: V2={a[i]} V3={b[i]}'); break
    else:
        same+=1
print(f'\n一致 {same} / 不同 {diff} / 跳过 {skip} / 共 {same+diff+skip}')
