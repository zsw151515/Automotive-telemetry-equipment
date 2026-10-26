"""检查原生交付文件中的关键对象，防止只验证运行时、漏存板框或覆铜。"""
from pathlib import Path
import collections
import hashlib
import json
import zipfile

base = Path(__file__).resolve().parent
expected_components = len(json.loads((base / "schematic-model.json").read_text("utf8"))["instances"])
errors = []
files = {}
legacy = base / 'eda/six-port-baseboard-v01.epro'
modern = base / 'eda/six-port-baseboard-v01.epro2'
with zipfile.ZipFile(legacy) as z:
    name = next(n for n in z.namelist() if n.endswith('.epcb'))
    text = z.read(name).decode('utf8')
    rows = [json.loads(l) for l in text.splitlines() if l]
    counts = collections.Counter(r[0] for r in rows)
    outlines = [r for r in rows if r[0] == 'POLY' and r[4] == 11]
    if counts['COMPONENT'] != expected_components or counts['POUR'] != 2:
        errors.append('legacy component/pour count incorrect')
    if len(outlines) != 1:
        errors.append('legacy must contain one closed board contour')
    else:
        p = outlines[0][6]
        if p[:2] != p[-2:]:
            errors.append('legacy board contour not closed')
        coords = p[:2] + p[3:]
        if (abs(max(coords[::2]) * .0254 - 150) > .001
                or abs(max(coords[1::2]) * .0254 - 75) > .001):
            errors.append('legacy board dimensions incorrect')
    for name in ['GND_L2_BASE', 'GND_BUCK_TOP']:
        if text.count(name) != 1:
            errors.append(f'legacy lost or duplicated pour {name}')
    files['epro'] = dict(bytes=legacy.stat().st_size,
                         sha256=hashlib.sha256(legacy.read_bytes()).hexdigest(),
                         components=counts['COMPONENT'], pours=counts['POUR'],
                         poured_fragments=counts['POURED'],
                         routed_line_primitives=counts['LINE'], outlines=len(outlines))

docs = collections.defaultdict(dict)
with zipfile.ZipFile(modern) as z:
    name = next(n for n in z.namelist() if n.endswith('.epru'))
    text = z.read(name).decode('utf8')
    current = None
    for line in text.splitlines():
        line = line.removesuffix('|')
        if '||' not in line:
            continue
        a, b = line.split('||', 1)
        try:
            meta, data = json.loads(a), json.loads(b)
        except json.JSONDecodeError:
            continue
        if not isinstance(meta, dict):
            continue
        if meta['type'] == 'DOCHEAD' and isinstance(data, dict):
            current = (data['docType'], data['uuid'])
        if current is None:
            continue
        key = (meta['type'], meta.get('id', meta['type']))
        if data is None:
            docs[current].pop(key, None)
        elif isinstance(data, dict):
            docs[current][key] = (meta, data)
pcbs = [v for (t, _), v in docs.items() if t == 'PCB']
if len(pcbs) != 1:
    errors.append('modern must contain one PCB')
else:
    pcb = pcbs[0]
    counts = collections.Counter(t for t, _ in pcb)
    pours = {m['id']: d for m, d in pcb.values() if m['type'] == 'POUR'}
    active_fills, stale_cache = 0, 0
    for m, d in pcb.values():
        if m['type'] == 'POURED':
            parent = json.loads(m['id'])[1]
            if parent in pours:
                active_fills += 1
            else:
                stale_cache += 1
    if counts['COMPONENT'] != expected_components or len(pours) != 2 or active_fills != 2:
        errors.append('modern component/pour/active-fill count incorrect')
    if {(d['name'], d['layerId'], d['netName']) for d in pours.values()} != {
            ('GND_L2_BASE', 15, 'GND'), ('GND_BUCK_TOP', 1, 'GND')}:
        errors.append('modern ground pour names/layers/nets incorrect')
    copper_lines = [d for m, d in pcb.values()
                    if m['type'] == 'LINE' and d.get('layerId') in [1, 2, 15, 16]]
    board_vias = [d for m, d in pcb.values() if m['type'] == 'VIA']
    if not copper_lines or min(d['width'] * .0254 for d in copper_lines) < .127 - .00001:
        errors.append('modern copper track below 0.127mm minimum')
    if not board_vias or any(d['holeDiameter'] * .0254 < .30 - .00001
                             or d['viaDiameter'] * .0254 < .60 - .00001
                             for d in board_vias):
        errors.append('modern board via below 0.30/0.60mm minimum')
    baseline = json.loads((base / 'exports/protected-copper-baseline.json').read_text('utf8'))
    line_by_id = {m['id']: d for m, d in pcb.values() if m['type'] == 'LINE'}
    via_by_id = {m['id']: d for m, d in pcb.values() if m['type'] == 'VIA'}
    preserved_board_vias = 0
    for old in baseline['lines']:
        actual_line = line_by_id.get(old['primitiveId'])
        fields = {'net': 'netName', 'layer': 'layerId', 'startX': 'startX',
                  'startY': 'startY', 'endX': 'endX', 'endY': 'endY', 'lineWidth': 'width'}
        for a, b in fields.items():
            before, after = old[a], actual_line.get(b) if actual_line else None
            same = (abs(before-after) < .001 if isinstance(before, (int, float))
                    and isinstance(after, (int, float)) else before == after)
            if not same:
                errors.append(f'prior copper changed: {old["primitiveId"]}.{a}')
    layout = json.loads((base / 'pcb-layout.json').read_text('utf8'))
    s3_id = next(p['pcb_id'] for p in layout['placements'] if p['ref'] == 'U5')
    for old in baseline['vias']:
        actual_via = via_by_id.get(old['primitiveId'])
        if not actual_via:
            if not old['primitiveId'].startswith(s3_id):
                errors.append(f'prior board via lost: {old["primitiveId"]}')
            continue  # 模块内12个过孔由下方封装检查独立验证。
        preserved_board_vias += 1
        if (old['net'] != actual_via['netName']
                or abs(old['x']-actual_via['centerX']) > .001
                or abs(old['y']-actual_via['centerY']) > .001):
            errors.append(f'prior board via changed: {old["primitiveId"]}')
    fp = docs.get(('FOOTPRINT', '55fb7e82f99e6970'), {})
    thermal_vias = [d for m, d in fp.values() if m['type'] == 'VIA']
    if len(thermal_vias) != 12 or any(
            abs(v['holeDiameter'] * .0254 - .30) > .0001
            or abs(v['viaDiameter'] * .0254 - .50) > .0001 for v in thermal_vias):
        errors.append('modern S3 thermal via dimensions incorrect')
    files['epro2'] = dict(bytes=modern.stat().st_size,
                          sha256=hashlib.sha256(modern.read_bytes()).hexdigest(),
                          components=counts['COMPONENT'], pours=len(pours),
                          active_poured_regions=active_fills,
                          orphan_fill_cache_records=stale_cache,
                          routed_line_primitives=len(copper_lines),
                          board_vias=len(board_vias),
                          minimum_track_width_mm=min(d['width'] * .0254 for d in copper_lines),
                          minimum_board_via_hole_mm=min(d['holeDiameter'] * .0254 for d in board_vias),
                          minimum_board_via_diameter_mm=min(d['viaDiameter'] * .0254 for d in board_vias),
                          preserved_prior_line_objects=len(baseline['lines']),
                          preserved_prior_board_vias=preserved_board_vias,
                          source_thermal_vias=len(thermal_vias))
report = {'passed': not errors, 'files': files, 'errors': errors,
          'scope': 'Native archive object persistence and selected geometry only; no fabrication approval'}
(base / 'exports/native-integrity-check.json').write_text(
    json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf8', newline='\n')
print(json.dumps(report, ensure_ascii=True))
raise SystemExit(0 if report['passed'] else 1)
