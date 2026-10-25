"""核对实际 PCB 读回的器件、焊盘网络和布局；不替代 DRC 或实物验收。"""
from pathlib import Path
import argparse
import hashlib
import json
from collections import Counter, defaultdict

parser = argparse.ArgumentParser()
parser.add_argument('readback', type=Path)
args = parser.parse_args()
base = Path(__file__).resolve().parent
model = json.loads((base / 'schematic-model.json').read_text('utf8'))
layout = json.loads((base / 'pcb-layout.json').read_text('utf8'))
raw = args.readback.read_bytes()
response = json.loads(raw)
actual = response['result'] if 'result' in response else response
expected = {c['ref']: c for c in model['instances']}
placements = {c['ref']: c for c in layout['placements']}
components = actual['components']
counts = Counter(c['ref'] for c in components)
errors = []
for ref in sorted(set(counts) | set(expected)):
    if counts[ref] != 1:
        errors.append(f'{ref}: expected one instance, found {counts[ref]}')
    if ref not in expected:
        errors.append(f'unexpected component: {ref}')
byid = {c['id']: c for c in components}
for c in components:
    p = placements.get(c['ref'])
    if not p:
        continue
    if c['id'] != p['pcb_id']:
        errors.append(f"{c['ref']}: PCB identity changed")
    if (abs(c['x'] * .0254 - p['x_mm']) > .005
            or abs(c['y'] * .0254 - p['y_mm']) > .005
            or abs((c['rotation'] - p['rotation'] + 180) % 360 - 180) > .001
            or c['layer'] != p['layer']):
        errors.append(f"{c['ref']}: placement differs from reviewed plan")
padnets = defaultdict(lambda: defaultdict(list))
for pad in actual['pads']:
    parent = byid.get(pad['primitiveId'][:16])
    if not parent:
        errors.append(f"unowned physical pad: {pad['primitiveId']}")
        continue
    ref, number = parent['ref'], str(pad['padNumber'])
    padnets[ref][number].append(pad.get('net') or '')
    wanted = expected[ref]['pins'].get(number, '')
    if (pad.get('net') or '') != wanted:
        errors.append(f"{ref}.{number}: actual {pad.get('net')!r}, expected {wanted!r}")
checked = 0
for ref, c in expected.items():
    for number in c['pins']:
        checked += 1
        if not padnets[ref].get(number):
            errors.append(f'missing physical pad: {ref}.{number}')
report = {
    'passed': not errors,
    'components': len(components),
    'physical_pads': len(actual['pads']),
    'assigned_logical_pins': checked,
    'readback_sha256': hashlib.sha256(raw).hexdigest(),
    'layout_sha256': hashlib.sha256((base / 'pcb-layout.json').read_bytes()).hexdigest(),
    'errors': errors,
    'scope': 'Actual PCB identities, pad nets and positions only; no routing or fabrication approval',
}
(base / 'exports/pcb-connectivity-check.json').write_text(
    json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf8', newline='\n')
print(json.dumps(report, ensure_ascii=True))
raise SystemExit(0 if report['passed'] else 1)
