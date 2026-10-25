"""用真实通孔焊盘读回，独立检查底面镜像和后盖投影所用坐标。"""
from pathlib import Path
import json
import math
import zipfile

base = Path(__file__).resolve().parent
layout = json.loads((base / 'pcb-layout.json').read_text('utf8'))
response = json.loads((base / 'exports/pcb-readback.json').read_text('utf8'))
actual = response.get('result', response)
byid = {c['id']: c['ref'] for c in actual['components']}
pads = {(byid[p['primitiveId'][:16]], str(p['padNumber'])): p for p in actual['pads']}
placements = {c['ref']: c for c in layout['placements']}
errors = []
maximum_error = 0
checked = 0
checked_bounds = 0
with zipfile.ZipFile(base / 'eda/six-port-baseboard-v01.epro') as archive:
    for ref in ['J1', 'J5', 'J6', 'J7', 'J8', 'J9', 'J10']:
        c = placements[ref]
        if c['layer'] != 2 or c['rotation'] % 360:
            errors.append(f'{ref}: expected reviewed bottom-side rotation zero')
            continue
        # 后盖使用的是包络中心；焊盘正确不能替代包络镜像正确。
        x0, x1, y0, y1 = layout['source_footprints'][ref]['bounds_mm']
        expected_bounds = [c['x_mm'] + x0, c['x_mm'] + x1,
                           c['y_mm'] - y1, c['y_mm'] - y0]
        if len(c['bounds_mm']) != 4 or any(abs(a-b) > .005
                                         for a, b in zip(c['bounds_mm'], expected_bounds)):
            errors.append(f'{ref}: bottom body bounds differ from reviewed transform')
        checked_bounds += 1
        fp = layout['source_footprints'][ref]['footprint_uuid']
        for line in archive.read('FOOTPRINT/' + fp + '.efoo').decode('utf8').splitlines():
            row = json.loads(line)
            if row[0] != 'PAD':
                continue
            p = pads[ref, str(row[5])]
            # 底面实际焊盘验证为 X 保留、局部 Y 取反；不是 X 取反。
            expected = (c['x_mm'] + row[6] * .0254, c['y_mm'] - row[7] * .0254)
            measured = (p['x'] * .0254, p['y'] * .0254)
            distance = math.dist(expected, measured)
            maximum_error = max(maximum_error, distance)
            checked += 1
            if distance > .005:
                errors.append(f'{ref}.{row[5]}: bottom transform differs from actual pad')
if checked != 26 or checked_bounds != 7:
    errors.append('expected 26 connector pads and 7 connector body bounds')
report = {'passed': not errors, 'checked_connector_pads': checked,
          'checked_connector_body_bounds': checked_bounds,
          'maximum_position_error_mm': maximum_error, 'errors': errors,
          'scope': 'Bottom connector pad/body transforms against actual CAD pads; no physical fit approval'}
(base / 'exports/mechanical-transform-check.json').write_text(
    json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf8', newline='\n')
print(json.dumps(report, ensure_ascii=True))
raise SystemExit(0 if report['passed'] else 1)
