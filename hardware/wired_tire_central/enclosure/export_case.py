"""导出试装样壳；只需 Python 标准库和 OpenSCAD 2021.01 或更新版。"""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict
import hashlib
import json
import math
from pathlib import Path
import re
import shutil
import struct
import subprocess

HERE = Path(__file__).resolve().parent
PARTS = ("body", "rear_cover", "rear_cover_pilot", "rear_cover_ports", "retainer")


def pcb_bottom_clearance(layout: dict | None = None, readback: dict | None = None) -> dict:
    """检查柱承压范围；包含DNP预留，不代表插头、线束或实物装配已验证。"""
    base = HERE.parent / 'baseboard'
    if layout is None:
        layout = json.loads((base / 'pcb-layout.json').read_text('utf8'))
    if readback is None:
        readback = json.loads((base / 'exports/pcb-readback.json').read_text('utf8'))
    readback = readback.get('result', readback)
    source = (HERE / 'central_case.scad').read_text('utf8')
    values = {}
    for key in ['case_w', 'board_w', 'board_h', 'board_y', 'board_hole_x',
                'board_hole_y', 'pcb_boss_d', 'board_standoff']:
        match = re.search(r'^' + key + r'\s*=\s*([0-9.]+)\s*;', source, re.M)
        if not match:
            raise ValueError(f'{key}: 支柱检查需要明确数值参数')
        values[key] = float(match.group(1))
    offset = [(values['case_w']-values['board_w'])/2,
              values['board_y']-values['board_h']/2]
    mounts = [[x-offset[0], y-offset[1]]
              for x in [(values['case_w']-values['board_hole_x'])/2,
                        (values['case_w']+values['board_hole_x'])/2]
              for y in [values['board_y']-values['board_hole_y']/2,
                        values['board_y']+values['board_hole_y']/2]]
    errors = []
    if layout['board_mm'] != [values['board_w'], values['board_h']]:
        errors.append('布局板框与支柱使用的SCAD尺寸不一致')
    rounded = lambda rows: sorted(tuple(round(v, 6) for v in row) for row in rows)
    if rounded(mounts) != rounded(layout['mounts_mm']):
        errors.append('SCAD实体支柱与PCB安装孔不对齐')
    placements = {p['ref']: p for p in layout['placements']}
    components = {c['ref']: c for c in readback['components']}
    if (len(placements) != len(layout['placements'])
            or len(components) != len(readback['components'])
            or placements.keys() != components.keys()):
        errors.append('布局与原生读回的器件集合不一致，不能使用过期读回验收')
    for ref in placements.keys() & components.keys():
        p, c = placements[ref], components[ref]
        if (p['layer'] != c['layer'] or abs(p['x_mm']-c['x']*.0254) > .005
                or abs(p['y_mm']-c['y']*.0254) > .005
                or abs(math.remainder(p['rotation']-c['rotation'], 360)) > .001):
            errors.append(f'{ref}: 布局与原生读回的位置、层或方向不一致')
    by_id = {c['id']: c for c in components.values()}
    pad_counts = Counter()
    pad_ids = [p['primitiveId'] for p in readback['pads']]
    if len(set(pad_ids)) != len(pad_ids):
        errors.append('原生读回存在重复实体焊盘，不能用重复项代替遗漏的焊脚')
    for pad in readback['pads']:
        owner = by_id.get(pad['primitiveId'][:16])
        if owner is None:
            errors.append('原生读回焊盘缺少对应器件，不能验收')
        else:
            pad_counts[owner['ref']] += 1
    if layout.get('source_footprints', {}).keys() != placements.keys():
        errors.append('布局缺少完整封装焊盘数量，不能核验原生读回是否过期')
    for ref, footprint in layout.get('source_footprints', {}).items():
        if pad_counts[ref] != footprint['physical_pads']:
            errors.append(f'{ref}: 原生读回的实体焊盘数量与布局封装不一致')
    radius, margin = values['pcb_boss_d']/2, .5
    result = dict(passed=False, errors=errors, pillar_diameter_mm=values['pcb_boss_d'],
                  pillar_height_mm=values['board_standoff'], clearance_mm=margin,
                  pillar_centres_pcb_mm=mounts, minimum_pillar_gap_mm=None,
                  checked_bottom_refs=[], checked_lead_pads=0, harness_unverified=True,
                  scope='底面本体保守包络、SMD焊脚及通孔焊脚对PCB支柱；不含插头和线束')
    if errors:
        return result
    minimum = math.inf

    def box_distance(bounds, point):
        x0, x1, y0, y1 = bounds
        return math.hypot(max(x0-point[0], 0, point[0]-x1),
                          max(y0-point[1], 0, point[1]-y1))

    def check(name, distance):
        nonlocal minimum
        for centre in mounts:
            d = distance(centre)
            minimum = min(minimum, d-radius)
            if d < radius+margin-1e-6:
                errors.append(f'{name}: 距支柱{centre}中心{d:.6f}mm，小于{radius+margin:.3f}mm')

    for p in placements.values():
        if p['layer'] == 2:
            result['checked_bottom_refs'].append(p['ref'])
            local = layout['source_footprints'][p['ref']].get('bounds_mm')
            if local is None:
                errors.append(p['ref']+' 源封装缺少本体包络，不能验收')
                continue
            x0, x1, y0, y1 = local
            angle = math.radians(p['rotation'])
            cs, sn = math.cos(angle), math.sin(angle)
            # 源封装为Top坐标：Bottom先镜像局部Y，再旋转、平移。
            points = [(p['x_mm']+x*cs+y*sn, p['y_mm']+x*sn-y*cs)
                      for x in [x0, x1] for y in [y0, y1]]
            bounds = [min(q[0] for q in points), max(q[0] for q in points),
                      min(q[1] for q in points), max(q[1] for q in points)]
            if any(abs(a-b) > .005 for a, b in zip(bounds, p['bounds_mm'])):
                errors.append(p['ref']+' 本体包络与当前源封装、位置和方向不一致')
            check(p['ref']+' 本体', lambda centre, bounds=bounds: box_distance(bounds, centre))
    for pad in readback['pads']:
        c = by_id.get(pad['primitiveId'][:16])
        if c is None:
            errors.append('原生读回焊盘缺少对应器件，不能验收')
            continue
        if pad['layer'] != 12 and not (c['layer'] == 2 and pad['layer'] == 2):
            continue
        result['checked_lead_pads'] += 1
        x, y = pad['x']*.0254, pad['y']*.0254
        shape, angle = pad['pad'], pad['rotation']
        if shape[0] not in ['RECT', 'ELLIPSE', 'OVAL']:
            raise ValueError(f'{c["ref"]}.{pad["padNumber"]}: 未支持的焊脚几何{shape[0]}')
        w, h = shape[1]*.0254, shape[2]*.0254
        if shape[0] == 'ELLIPSE' and abs(w-h) < 1e-8:
            distance = lambda centre, x=x, y=y, r=w/2: max(0, math.hypot(centre[0]-x, centre[1]-y)-r)
        else:
            # 旋转局部外接矩形再取全局AABB，保守包住圆角、椭圆及椭圆长孔。
            cs, sn = math.cos(angle), math.sin(angle)
            points = [(x+dx*cs-dy*sn, y+dx*sn+dy*cs)
                      for dx in [-w/2, w/2] for dy in [-h/2, h/2]]
            bounds = [min(q[0] for q in points), max(q[0] for q in points),
                      min(q[1] for q in points), max(q[1] for q in points)]
            distance = lambda centre, bounds=bounds: box_distance(bounds, centre)
        check(f'{c["ref"]}.{pad["padNumber"]}', distance)
    result.update(passed=not errors, minimum_pillar_gap_mm=None if math.isinf(minimum) else minimum)
    return result


def pcb_mechanical() -> tuple[str, dict]:
    """只读取已审查的布局坐标；不能把此投影当成实物对插通过。"""
    path = HERE.parent / 'baseboard/pcb-layout.json'
    raw = path.read_bytes()
    layout = json.loads(raw)
    placements = {p['ref']: p for p in layout['placements']}
    refs = ['J5', 'J6', 'J7', 'J8', 'J9', 'J10', 'J1']
    labels = ['FL', 'FR', 'RL', 'RR', 'BL', 'BR', '12V']
    source = (HERE / 'central_case.scad').read_text(encoding='utf8')
    values = {}
    for key in ['case_w', 'board_w', 'board_h', 'board_y']:
        m = re.search(r'^' + key + r'\s*=\s*([0-9.]+)\s*;', source, re.M)
        if not m:
            raise ValueError(f'{key}: 机械投影需要明确数值参数')
        values[key] = float(m.group(1))
    if layout['board_mm'] != [values['board_w'], values['board_h']]:
        raise ValueError('SCAD与实际PCB板框尺寸不一致')
    offset = [(values['case_w']-values['board_w'])/2, values['board_y']-values['board_h']/2]
    centres = []
    for ref in refs:
        p = placements[ref]
        if p['layer'] != 2 or p['rotation'] % 360:
            raise ValueError(f'{ref}: 连接器层/方向改变，需重新审查后盖投影')
        x0, x1, y0, y1 = p['bounds_mm']
        centres.append([round((x0+x1)/2+offset[0], 3), round((y0+y1)/2+offset[1], 3)])
    windows = [[12, 12]] * 6 + [[9, 12]]
    text = ('// 当前PCB连接器外壳包络中心投影，非厂商面板开孔规范。\n'
            'port_centres = ' + json.dumps(centres) + ';\n'
            'port_label = ' + json.dumps(labels) + ';\n'
            'port_windows = ' + json.dumps(windows) + ';\n')
    data = {'layout_sha256': hashlib.sha256(raw).hexdigest(), 'refs': refs,
            'labels': labels, 'board_offset_mm': offset, 'centres_mm': centres,
            'prototype_windows_mm': windows, 'physically_verified': False}
    return text, data


def inspect_stl(path: Path) -> dict:
    """检查二进制STL的边闭合、法向一致性、连通体和正体积。"""
    data = path.read_bytes()
    count = struct.unpack_from("<I", data, 80)[0]
    if len(data) != 84 + 50 * count or count == 0:
        raise ValueError(f"{path.name}: 不是非空二进制STL")
    vertices: list[tuple[float, float, float]] = []
    vertex_ids: dict[tuple[float, float, float], int] = {}
    edges: dict[tuple[int, int], list[tuple[int, int]]] = defaultdict(list)
    neighbors: list[set[int]] = [set() for _ in range(count)]
    volume = 0.0
    for face in range(count):
        coords = struct.unpack_from("<12fH", data, 84 + face * 50)
        points = [tuple(round(v, 5) for v in coords[i:i + 3]) for i in (3, 6, 9)]
        ids = []
        for point in points:
            if point not in vertex_ids:
                vertex_ids[point] = len(vertices)
                vertices.append(point)
            ids.append(vertex_ids[point])
        if len(set(ids)) != 3:
            raise ValueError(f"{path.name}: 退化三角形 {face}")
        a, b, c = points
        cross = (b[1]*c[2]-b[2]*c[1], b[2]*c[0]-b[0]*c[2], b[0]*c[1]-b[1]*c[0])
        volume += sum(a[i] * cross[i] for i in range(3)) / 6
        for u, v in zip(ids, ids[1:] + ids[:1]):
            edges[min(u, v), max(u, v)].append((face, 1 if u < v else -1))
    for uses in edges.values():
        if len(uses) != 2 or uses[0][1] + uses[1][1] != 0:
            raise ValueError(f"{path.name}: 非闭合边或法向不一致")
        f, g = uses[0][0], uses[1][0]
        neighbors[f].add(g)
        neighbors[g].add(f)
    reached = {0}
    pending = [0]
    while pending:
        for face in neighbors[pending.pop()]:
            if face not in reached:
                reached.add(face)
                pending.append(face)
    if len(reached) != count or volume <= 0:
        raise ValueError(f"{path.name}: 存在分离实体或体积非正")
    lower = [min(v[i] for v in vertices) for i in range(3)]
    upper = [max(v[i] for v in vertices) for i in range(3)]
    return {
        "triangles": count, "closed_edges": True, "consistent_winding": True,
        "connected_components": 1, "volume_mm3": round(volume, 3),
        "bounds_mm": [lower, upper],
        "size_mm": [round(upper[i]-lower[i], 5) for i in range(3)],
        "sha256": hashlib.sha256(data).hexdigest(),
    }


def run(command: list[str]) -> str:
    result = subprocess.run(command, capture_output=True, text=True, encoding="utf-8", errors="replace")
    output = result.stdout + result.stderr
    if result.returncode or "ERROR:" in output or "WARNING:" in output:
        raise RuntimeError(f"OpenSCAD 导出失败：{output}")
    return output.strip()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--openscad", help="openscad.com / openscad 可执行文件路径")
    parser.add_argument("--check-only", action="store_true", help="只检查现有STL，不写文件")
    args = parser.parse_args()
    out = HERE / "exports"
    source = HERE / "central_case.scad"
    report = {"scope": "mechanical fit prototype; not physically validated", "unit": "mm"}
    mechanical_text, mechanical_data = pcb_mechanical()
    bottom_clearance = pcb_bottom_clearance()
    if not bottom_clearance['passed']:
        raise ValueError('底板与后盖支柱干涉或输入过期：' + '; '.join(bottom_clearance['errors']))
    report['pcb_bottom_clearance'] = bottom_clearance
    mechanical_file = HERE / 'pcb_mechanical.scad'
    if args.check_only:
        if mechanical_file.read_text(encoding='utf8') != mechanical_text:
            raise ValueError('底板连接器位置已改变，请重新生成外壳')
    else:
        mechanical_file.write_text(mechanical_text, encoding='utf8', newline='\n')
    report['pcb_projection'] = mechanical_data
    if not args.check_only:
        exe = args.openscad or shutil.which("openscad.com") or shutil.which("openscad")
        if not exe:
            parser.error("请安装OpenSCAD或传入 --openscad；不会自动下载或安装")
        exe = str(Path(exe).resolve())
        report["tool"] = run([exe, "--version"])
        out.mkdir(exist_ok=True)
        for part in PARTS:
            log = run([exe, "--hardwarnings", "--export-format", "binstl", "-D", f'part="{part}"',
                       "-o", str(out / f"{part}.stl"), str(source)])
            print(f"Exported {part}.stl")
        for name, part, camera in (
            ("front", "assembly", "285,-165,-220,95,62.5,25"),
            ("exploded", "exploded", "300,-185,230,95,62.5,50"),
        ):
            run([exe, "--hardwarnings", "--preview", "--imgsize", "1500,1000", "--viewall",
                 "--autocenter", "--projection=o", "--colorscheme=Tomorrow", "--camera", camera,
                 "-D", f'part="{part}"', "-o", str(out / f"{name}.png"), str(source)])
            print(f"Rendered {name}.png")
    report["source_sha256"] = hashlib.sha256(source.read_bytes()).hexdigest()
    report["parts"] = {part: inspect_stl(out / f"{part}.stl") for part in PARTS}
    if args.check_only:
        saved = json.loads((out / "geometry_report.json").read_text(encoding="utf-8"))
        if saved.get('pcb_projection') != report['pcb_projection']:
            raise ValueError('底板投影记录已过期，请重新生成外壳')
        if saved.get('pcb_bottom_clearance') != bottom_clearance:
            raise ValueError('底面元件与支柱校验记录已过期，请重新生成外壳')
        if saved["source_sha256"] != report["source_sha256"]:
            raise ValueError("SCAD与导出记录不一致，请重新生成")
        for part in PARTS:
            if saved["parts"][part] != report["parts"][part]:
                raise ValueError(f"{part}.stl 与导出记录不一致，请重新生成")
        report["matches_saved_report"] = True
    else:
        (out / "geometry_report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps(report, ensure_ascii=True, indent=2))


if __name__ == "__main__":
    main()
