"""从设计候选生成便于审查的GPIO表，不修改任何固件profile。"""
from pathlib import Path
import csv,json

base=Path(__file__).resolve().parent
m=json.loads((base/'schematic-model.json').read_text('utf8'))
rows=[]
c3={0:12,1:13,2:5,3:6,4:18,5:19,6:20,7:21,8:22,9:23,10:16,18:26,19:27,20:30}
s3={0:27,1:39,2:38,4:4,5:5,6:6,7:7,10:18,11:19,12:20,13:21,14:22,15:8,16:9,17:10,19:13,20:14,21:23}
for i in m['instances']:
    if i['part'] not in ('c3','s3'):continue
    for gpio,pad in (c3 if i['part']=='c3' else s3).items():
        net=i['pins'].get(str(pad))
        if net:rows.append([i['ref'],i['part'].upper(),gpio,pad,net,'','3.3V','-1/未核准','本次设计候选，未启用profile'])
p4=[('SCK',30,'P3-4'),('MOSI',29,'P3-5'),('MISO',31,'P3-3'),('CS_FL',28,'P3-6'),('CS_FR',5,'P3-7'),('CS_RL',4,'P3-8'),('CS_RR',3,'P3-9'),('CS_BRAKE',2,'P3-10'),('MUX_A',52,'P1-1'),('MUX_B',51,'P1-2'),('MUX_C',50,'P1-3'),('MUX_Y',49,'P1-4')]
for name,gpio,pin in p4:rows.append(['7B','ESP32-P4',gpio,'',name,pin,'3.3V','-1/未核准','本次设计候选；实板排针方向与修订待核对'])
with (base/'pinmap.csv').open('w',encoding='utf-8-sig',newline='') as f:
    writer=csv.writer(f);writer.writerow(['位号','MCU','GPIO','模块焊盘','网络或功能','7B排针','电平','旧状态','新状态']);writer.writerows(rows)
print(f'Wrote {len(rows)} candidate GPIO mappings')
