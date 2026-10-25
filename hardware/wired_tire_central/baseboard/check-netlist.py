"""核对EDA实际导出网表；本脚本不修改图纸或放行硬件。"""
from pathlib import Path
import argparse,json,hashlib
from collections import Counter

p=argparse.ArgumentParser();p.add_argument('netlist',type=Path);args=p.parse_args()
base=Path(__file__).resolve().parent
model=json.loads((base/'schematic-model.json').read_text('utf8'))
parts=json.loads((base/'parts.json').read_text('utf8'))
raw=args.netlist.read_bytes();actual=json.loads(raw)
components=actual['components'];counts=Counter(c['props'].get('Designator') for c in components.values())
byref={c['props']['Designator']:c for c in components.values() if c['props'].get('Designator')}
errors=[];checked=0
expected={i['ref'] for i in model['instances']}
for ref,n in counts.items():
    if ref and n!=1:errors.append(f'duplicate reference {ref}: {n}')
for ref in sorted(set(byref)-expected):errors.append(f'unexpected device {ref}')
for i in model['instances']:
    ref=i['ref'];c=byref.get(ref)
    if c is None:errors.append(f'missing device {ref}');continue
    part=parts[i['part']];props=c['props'];pins=c.get('pinInfoMap',{})
    for key,value in [('Manufacturer Part',part['manufacturerId']),('Supplier Part',part['supplierId'])]:
        if props.get(key)!=value:errors.append(f'{ref} {key}: {props.get(key)!r} != {value!r}')
    wanted_bom='no' if i['dnp'] else 'yes'
    if props.get('Add into BOM')!=wanted_bom:
        errors.append(f'{ref} Add into BOM: {props.get("Add into BOM")!r} != {wanted_bom!r}')
    if props.get('Convert to PCB')!='yes':errors.append(f'{ref} must retain its PCB footprint')
    for pad,net in i['pins'].items():
        received=pins.get(pad,{}).get('net')
        if received!=net:errors.append(f'{ref}.{pad}: {received!r} != {net!r}')
        checked+=1

def net(ref,pad):return byref.get(ref,{}).get('pinInfoMap',{}).get(str(pad),{}).get('net','')
# Independent safety checks: catches model edits that would evade a simple model/export comparison.
for ref,pad,value in [('D3',1,'GND'),('D3',2,'12V_PROTECTED'),('D4',1,'BUZZER_LOW'),('D4',2,'5V_BUCK'),('Q3',9,'5V_SCREEN'),('U32',9,'GND'),('U33',9,'GND'),('U31',16,'MAIN_N1'),('U5',39,'PORT_FAULT_N'),('U5',38,'MAIN_FAULT_N')]:
    if net(ref,pad)!=value:errors.append(f'safety net {ref}.{pad} must be {value}')
if net('U31',25):errors.append('LM74900 RTN/EP must remain floating')
if net('U35',1)==net('U35',23):errors.append('baseboard 3V3 and P4_3V3_REF must not be joined')
for ref in ['U1','U2','U3','U4']:
    if net(ref,3)!='3V3':errors.append(ref+' module supply incorrect')
    if not net(ref,30).endswith('_ARM'):errors.append(ref+' GPIO20 must be dedicated ARM')
for ref in ['U25','U26','U27','U28','U29','U30']:
    if net(ref,8)!='12V_PROTECTED' or net(ref,9)!='GND':errors.append(ref+' power/EP incorrect')

report={'passed':not errors,'checked_components':len(expected),'checked_pin_nets':checked,
        'netlist_sha256':hashlib.sha256(raw).hexdigest(),
        'model_sha256':hashlib.sha256((base/'schematic-model.json').read_bytes()).hexdigest(),
        'errors':errors,'scope':'CAD connectivity and catalogue metadata only; not circuit, routing or hardware approval'}
out=base/'exports/netlist-check.json';out.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n','utf8')
print(json.dumps({**report,'errors':errors[:25]},ensure_ascii=True))
raise SystemExit(0 if report['passed'] else 1)
