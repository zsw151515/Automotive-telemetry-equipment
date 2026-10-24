/* 7B + 六接口底板：长方形试装样壳，单位 mm。
 * 官方玻璃/孔距作为参考；底板、连接器及玻璃相对PCB位置尚待实测。
 * 不包含电子制造设计。修改参数后必须重新导出并检查。
 * part: body / rear_cover / rear_cover_pilot / retainer / assembly / exploded
 */
part = "assembly";
$fn = 48;
eps = 0.05;

case_w = 190;
case_h = 125;
case_d = 60;
wall = 3;
front_t = 3;
rear_t = 3;
corner_r = 3;

// 微雪官方外形图（20251108）：不是打印开孔尺寸。
glass_w = 164.28;
glass_h = 99.17;
display_d = 15.3;
va_w = 154.58;
va_h = 86.42;
va_left = 3.06;
va_top = 4.02;
glass_clearance = 0.5; // 单边；玻璃外形公差 +/-0.1 另见说明。
foam_t = 0.5;        // 装配示意值；软垫厚度/压缩量到货后确定。
window_w = 161;
window_h = 94;
screen_hole_x = 156;
screen_hole_y = 89;
screen_hole_clearance = 4.5; // M2.5配垫片，给玻璃与PCB偏移留调整量。

// 仅用于安排空间：不是已经设计完成的六接口PCB。
board_w = 150;
board_h = 75;
board_t = 1.6;
board_y = 70;
board_hole_x = 140;
board_hole_y = 65;
board_component_h = 14;
board_standoff = 10;

boss_d = 10;
pilot_d = 2.6; // M3塑料自攻试孔；不是热熔螺母孔。
screw_clearance = 3.4;
cover_screw_inset = 7.5;
retainer_z = 24;
retainer_t = 3;
retainer_w = 10;
port_y = 15;
port_x = [23, 47, 71, 95, 119, 143, 167];
port_label = ["T1", "T2", "T3", "T4", "B1", "B2", "P"];
pilot_port_d = 3; // 只定位，不代表成品连接器孔径/电气定义。

glass_x = (case_w-glass_w)/2;
glass_y = (case_h-glass_h)/2;
body_d = case_d-rear_t;
screen_rows = [(case_h-screen_hole_y)/2, (case_h+screen_hole_y)/2];
case_cols = [cover_screw_inset, case_w-cover_screw_inset];
retainer_cols = [7, case_w-7]; // 与玻璃定位框保留间隙，不能直接沿用后盖柱位置。
cover_rows = [cover_screw_inset, case_h-cover_screw_inset];
board_cols = [(case_w-board_hole_x)/2, (case_w+board_hole_x)/2];
board_rows = [board_y-board_hole_y/2, board_y+board_hole_y/2];
board_front = body_d-board_standoff-board_t;

assert(window_w < glass_w && window_h < glass_h, "前唇必须压在玻璃黑边范围");
assert(glass_x+va_left > (case_w-window_w)/2+glass_clearance+0.3,
       "左VA余量不足");
assert(glass_y+glass_h-va_top < (case_h+window_h)/2-glass_clearance-0.3,
       "上VA余量不足");
assert(glass_x+va_left+va_w < (case_w+window_w)/2-glass_clearance-0.3,
       "右VA余量不足");
assert(glass_y+glass_h-va_top-va_h > (case_h-window_h)/2+glass_clearance+0.3,
       "下VA余量不足");
assert(board_w < case_w-2*wall && board_y+board_h/2 < case_h-wall,
       "底板包络越出内腔");
assert(board_front-board_component_h > retainer_z+retainer_t+2,
       "底板元件与屏幕固定条间隙不足");

module rounded_plate(w,h,t,r=3) {
    hull() for(x=[r,w-r],y=[r,h-r]) translate([x,y,0]) cylinder(r=r,h=t);
}

module body() {
    difference() {
        union() {
            difference() {
                rounded_plate(case_w,case_h,body_d,corner_r);
                translate([wall,wall,front_t])
                    cube([case_w-2*wall,case_h-2*wall,body_d+eps]);
                translate([(case_w-window_w)/2,(case_h-window_h)/2,-eps])
                    cube([window_w,window_h,front_t+2*eps]);
            }
            // 前面承托唇 + 背面浅定位框，屏幕由后方M2.5安装位固定。
            translate([glass_x-glass_clearance-2,glass_y-glass_clearance-2,front_t-eps])
            difference() {
                cube([glass_w+2*glass_clearance+4,glass_h+2*glass_clearance+4,3+eps]);
                translate([2,2,-eps]) cube([glass_w+2*glass_clearance,
                    glass_h+2*glass_clearance,3+3*eps]);
            }
            for(x=case_cols,y=cover_rows)
                translate([x,y,front_t-eps]) cylinder(d=boss_d,h=body_d-front_t+eps);
            for(x=retainer_cols,y=screen_rows)
                translate([x,y,front_t-eps]) cylinder(d=boss_d,h=retainer_z-front_t+eps);
        }
        for(x=case_cols,y=cover_rows)
            translate([x,y,body_d-12]) cylinder(d=pilot_d,h=12+eps);
        for(x=retainer_cols,y=screen_rows)
            translate([x,y,retainer_z-12]) cylinder(d=pilot_d,h=12+eps);
    }
}

// 打印局部坐标：平面z=0..rear_t，柱朝+z，y与装配坐标反向。
// 装配仅用绕x轴翻转180度的刚体变换，不能用镜像替代翻面。
module rear_cover(with_pilots=false) {
    difference() {
        union() {
            rounded_plate(case_w,case_h,rear_t,corner_r);
            for(x=board_cols,y=board_rows)
                translate([x,case_h-y,rear_t-eps]) cylinder(d=8,h=board_standoff+eps);
        }
        for(x=case_cols,y=cover_rows)
            translate([x,y,-eps]) cylinder(d=screw_clearance,h=rear_t+2*eps);
        for(x=board_cols,y=board_rows)
            translate([x,case_h-y,rear_t+board_standoff-8]) cylinder(d=pilot_d,h=8+eps);
        if(with_pilots) for(i=[0:len(port_x)-1]) {
            translate([port_x[i],case_h-port_y,-eps]) cylinder(d=pilot_port_d,h=rear_t+2*eps);
            // 文字从外面看正向；外面是本零件的z=0面。
            translate([port_x[i],case_h-port_y-6,0.5]) rotate([180,0,0])
                linear_extrude(height=0.5+eps)
                    text(port_label[i],size=3.5,halign="center",valign="center");
        }
    }
}

// 打印2件。条上大孔是M2.5调整孔；两端小孔固定到壳体。
module retainer() {
    difference() {
        // 截平圆条两端，避开主壳侧墙；保留0.5单边插装间隙。
        intersection() {
            hull() for(x=retainer_cols) translate([x,0,0]) cylinder(d=retainer_w,h=retainer_t);
            translate([wall+0.5,-retainer_w/2,-eps])
                cube([case_w-2*wall-1,retainer_w,retainer_t+2*eps]);
        }
        for(x=retainer_cols) translate([x,0,-eps]) cylinder(d=screw_clearance,h=retainer_t+2*eps);
        for(x=[(case_w-screen_hole_x)/2,(case_w+screen_hole_x)/2])
            translate([x,0,-eps]) cylinder(d=screen_hole_clearance,h=retainer_t+2*eps);
    }
}

module references(explode=0) {
    // 简化包络，不是微雪的PCB/连接器3D模型。VA按实际偏置绘制。
    color([0.06,0.07,0.09]) translate([glass_x,glass_y,front_t+foam_t])
        cube([glass_w,glass_h,2]);
    color([0.06,0.52,0.67]) translate([glass_x+va_left,
        glass_y+glass_h-va_top-va_h,front_t+foam_t-eps]) cube([va_w,va_h,0.1]);
    color([0.05,0.35,0.18,0.45]) translate([(case_w-164)/2,(case_h-97)/2,front_t+foam_t+2])
        cube([164,97,display_d-2]);
    color([0.1,0.45,0.24]) translate([(case_w-board_w)/2,board_y-board_h/2,board_front+explode])
        cube([board_w,board_h,board_t]);
    color([0.2,0.6,0.3,0.18]) translate([(case_w-board_w)/2,board_y-board_h/2,
        board_front-board_component_h+explode]) cube([board_w,board_h,board_component_h]);
}

module assembly(explode=0) {
    color([0.22,0.25,0.30,explode > 0 ? 0.35 : 1]) body();
    references(explode);
    color([0.85,0.58,0.15]) for(y=screen_rows)
        translate([0,y,retainer_z]) retainer();
    color([0.45,0.49,0.55]) translate([0,case_h,case_d+explode])
        rotate([180,0,0]) rear_cover(true);
}

if(part=="body") body();
else if(part=="rear_cover") rear_cover(false);
else if(part=="rear_cover_pilot") rear_cover(true);
else if(part=="retainer") translate([-wall-0.5,retainer_w/2,0]) retainer();
else if(part=="assembly") assembly();
else if(part=="exploded") assembly(40);
else assert(false,"未知part");
