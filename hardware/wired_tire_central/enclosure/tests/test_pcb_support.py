"""实际 SH6 封装与样壳支柱干涉的回归场景；不验证电气可行性。"""
import copy
import json
import math
from pathlib import Path
import sys
import unittest

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
import export_case


class PcbSupportTests(unittest.TestCase):
    def setUp(self):
        self.scene = json.loads((HERE / 'fixtures/j14-sh6-bottom.json').read_text('utf8'))

    def check_scene(self):
        check = getattr(export_case, 'pcb_bottom_clearance', None)
        self.assertTrue(callable(check), '缺少底面元件与实际 PCB 支柱的装配检查')
        return check(self.scene['layout'], self.scene['readback'])

    def move_header(self, dx, dy):
        p = self.scene['layout']['placements'][0]
        p['x_mm'] += dx
        p['y_mm'] += dy
        p['bounds_mm'] = [v + (dx if i < 2 else dy) for i, v in enumerate(p['bounds_mm'])]
        c = self.scene['readback']['components'][0]
        c['x'] += dx / .0254
        c['y'] += dy / .0254
        for pad in self.scene['readback']['pads']:
            pad['x'] += dx / .0254
            pad['y'] += dy / .0254

    def test_original_j14_hits_eight_mm_pillar(self):
        result = self.check_scene()
        self.assertFalse(result['passed'])
        self.assertTrue(any('J14' in e for e in result['errors']))

    def test_mechanical_shift_clears_pillar_only(self):
        self.move_header(-1.7, -1.7)
        result = self.check_scene()
        self.assertTrue(result['passed'], result['errors'])
        self.assertGreaterEqual(result['minimum_pillar_gap_mm'], .5)
        self.assertTrue(result['harness_unverified'])

    def test_ground_mounting_tab_is_not_ignored(self):
        self.move_header(-1.7, -1.7)
        tab = next(p for p in self.scene['readback']['pads'] if str(p['padNumber']) == '7')
        tab.update(x=145 / .0254, y=70 / .0254)
        result = self.check_scene()
        self.assertFalse(result['passed'])
        self.assertTrue(any('J14.7' in e for e in result['errors']))

    def test_stale_readback_cannot_hide_added_parts(self):
        self.move_header(-1.7, -1.7)
        extra = copy.deepcopy(self.scene['layout']['placements'][0])
        extra['ref'] = 'R140'
        self.scene['layout']['placements'].append(extra)
        result = self.check_scene()
        self.assertFalse(result['passed'])
        self.assertTrue(any('读回' in e for e in result['errors']))

    def test_stale_pad_list_cannot_hide_a_mounting_tab(self):
        self.move_header(-1.7, -1.7)
        self.scene['layout']['source_footprints'] = {'J14': {'physical_pads': 8}}
        self.scene['readback']['pads'].pop()
        result = self.check_scene()
        self.assertFalse(result['passed'])
        self.assertTrue(any('读回' in e for e in result['errors']))

    def test_stale_body_bounds_cannot_hide_pillar_collision(self):
        self.move_header(-1.7, -1.7)
        p = self.scene['layout']['placements'][0]
        old_x, old_y = p['x_mm'], p['y_mm']
        # 实际旋转和移动元件、全部焊盘；故意保留旧的安全本体缓存。
        for pad in self.scene['readback']['pads']:
            pad['x'] = (145-(pad['x']*.0254-old_x))/.0254
            pad['y'] = (63.2-(pad['y']*.0254-old_y))/.0254
            pad['rotation'] -= math.pi
        p.update(x_mm=145, y_mm=63.2, rotation=0)
        self.scene['readback']['components'][0].update(x=145/.0254, y=63.2/.0254, rotation=0)
        result = self.check_scene()
        self.assertFalse(result['passed'])
        self.assertTrue(any('本体' in e for e in result['errors']))

    def test_duplicate_pad_cannot_replace_a_mounting_tab(self):
        self.move_header(-1.7, -1.7)
        self.scene['readback']['pads'][-1] = copy.deepcopy(self.scene['readback']['pads'][0])
        result = self.check_scene()
        self.assertFalse(result['passed'])
        self.assertTrue(any('重复' in e for e in result['errors']))


if __name__ == '__main__':
    unittest.main()
