"""核对维护USB的实际串联路径及DNP电容；不代替阻抗/布线/实测检查。"""
from pathlib import Path
import argparse,json,re
p=argparse.ArgumentParser();p.add_argument('--group',choices=['FL','FR','RL','RR','BRAKE'],action='append');a=p.parse_args();b=Path(__file__).resolve().parent
spec=json.loads((b/'usb-interfaces.json').read_text('utf8'));data=json.loads((b/'exports/six-port-v01.enet').read_text('utf8'));parts={c['props'].get('Designator'):c for c in data['components'].values()};errors=[];selected=a.group or list(spec['groups'])
def net(ref,pin):return parts.get(ref,{}).get('pinInfoMap',{}).get(str(pin),{}).get('net','')
for g in selected:
 s=spec['groups'][g]
 for leg,n in [('DM',1),('DP',2)]:
  r=s['resistors'][leg];c=s['capacitors'][leg];chip=f'{g}_USB_{leg}_MCU';bus=f'{g}_USB_{leg}'
  for ref,pin,want in [(s['mcu'],s['mcu_pads'][leg],chip),(r,1,chip),(r,2,bus),(c,1,bus),(c,2,'GND'),(s['header'],n,bus)]:
   if net(ref,pin)!=want:errors.append(f'{g}: {ref}.{pin} must be {want}, got {net(ref,pin)!r}')
  if r in parts:
   props=parts[r]['props'];value=re.sub(r'[^0-9.kKmM]','',str(props.get('Value','')))
   if value not in ['33','33.0']:errors.append(r+' must be33ohm')
   if props.get('Add into BOM')!='yes':errors.append(r+' series resistor must be populated')
  if c in parts:
   props=parts[c]['props']
   if props.get('Add into BOM')!='no' or props.get('Convert to PCB')!='yes':errors.append(c+' must retain PCB footprint and be excluded from BOM')
 for pin,want in [(3,'GND'),(4,g+'_EN'),(5,'GND'),(6,g+'_BOOT')]:
  if net(s['header'],pin)!=want:errors.append(f'{s["header"]}.{pin} changed')
result=dict(passed=not errors,checked_groups=selected,all_groups_checked=len(selected)==5,errors=errors,scope='Actual schematic USB circuit topology and DNP flags only; no USB routing or hardware approval')
(b/'exports/usb-interface-check.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf8',newline='\n');print(json.dumps(result,ensure_ascii=True));raise SystemExit(bool(errors))
