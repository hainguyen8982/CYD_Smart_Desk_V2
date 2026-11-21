#!/usr/bin/env python3
"""
CYD SmartDesk V2 — V8 Seamless Front Cap + 34° Wedge + 4-Screw M3x9 Sandwich
Target Hardware : NiceMCU-32S-DEV 2.8" IPS (86x50mm PCB, 87mm w/ USB-C)
Design Features :
  - Seamless Braun/Apple front bezel (NO seam on front face, matching concept render!)
  - Crisp 45° front bevel (2.0mm), symmetrical top & bottom bezels
  - Smooth R2.8mm rounded fillet on rear face perimeter ONLY
  - 34° ergonomic tilt angle (matching pendlerbuddy.3mf)
  - 4x M3x9mm (9mm thread / 12mm total) PendlerBuddy-style sandwich clamping:
      1) smartdesk_v2_front.stl : Full-width seamless chamfered front cap (split at w=-8.6mm)
         with exact 60.0x44.5mm display window, LDR pinhole on right bezel, PCB pocket,
         4x M3 self-tapping bosses (Ø2.75mm pilot, 78x42mm pitch), and stepped alignment lip.
      2) NiceMCU 2.8" IPS PCB   : Drops straight into the front cap PCB pocket.
      3) smartdesk_v2_rear.stl  : 34° wedge body with solid 2.2mm bottom floor, 4 internal
         hollow screw pillars (2.8mm seat + Ø6.6mm counterbore tunnel exiting rear/bottom),
         14.0x5.5mm snap-in USB-C cutout, 8Ω 1W speaker grille + retaining ring (right),
         4 ESP32 vent slots (left), 4 silicone foot recesses.
  - 100% support-free FDM printing!
"""

import math
import os
import struct
import numpy as np
from manifold3d import Manifold, CrossSection

# ── MEASURED PCB PARAMETERS (NiceMCU-32S 2.8" IPS) ──────────────────────
PCB_W          = 86.0
PCB_USB_EXTRA  = 1.2     # On-board USB-C overhang on left side (front view)
PCB_H          = 50.0
PCB_T          = 1.6
GLASS_T        = 3.8     # Glass height above PCB
HOLE_DX        = 78.0    # Mounting hole horizontal pitch
HOLE_DY        = 42.0    # Mounting hole vertical pitch
TOL            = 0.35    # FDM 3D print clearance
# Measured on real NiceMCU 2.8" IPS:
#   Left USB-C tip -> glass: 10.0mm | Left black chin: 8.6mm | Active: ~58-60mm
#   Right black border: 2.6mm | Glass -> right PCB edge: 8.0mm
#   => Active display center is +3.4mm right of 86mm PCB center.
#   To center the display at u = 0.0 (100% symmetrical left & right bezels!),
#   shift the 86mm PCB center to u = -3.4mm inside the enclosure!
PCB_CX         = -3.4    # 86mm PCB horizontal center (aligns active display to exact u = 0.0!)

# ── DISPLAY WINDOW & LDR SENSOR (Front Face Local Coords) ───────────────
WIN_W          = 60.0    # Measured active display window width (mm)
WIN_H          = 44.5    # Measured active display window height (mm)
WIN_CX         = 0.0     # DEAD CENTER of front face -> 100% symmetrical left = right!
WIN_R          = 1.6
LDR_FROM_RIGHT = 4.0
LDR_FROM_TOP   = 11.0
LDR_DIA        = 3.0

# ── OUTER 20° WEDGE GEOMETRY (Desk Coords: X=Right, Y=Back, Z=Up) ───────
OUTER_W        = 101.0   # Golden-ratio symmetrical width (20.5mm left = 20.5mm right bezel, >=2.55mm left wall)
FRONT_H        = 61.0    # Symmetrical slim front height along 20° face (keeps PCB cleanly above Z=2.2mm floor)
TILT_DEG       = 20.0    # Ergonomic 20° backward tilt (matches concept render, zero ceiling glare!)
TOP_SLOPE_DEG  = 15.0    # Sculpted 15° backward roof taper flowing smoothly along R7.0mm corners
DEPTH          = 40.0    # Sleek tapered base depth on desk (mm)
CORNER_R       = 7.0     # Soft Apple/Braun rounded 2D corners (identical top & bottom -> zero crease!)
FRONT_CHAMFER  = 1.6     # Crisp fine 45° bevel on front face perimeter
REAR_FILLET    = 3.5     # Uniform smooth R3.5mm rounded fillet on ALL 4 rear edges
WALL           = 2.6     # Outer side/top wall thickness (mm)
BOTTOM_WALL    = 2.2     # Solid flat bottom desk floor thickness (mm)
REAR_WALL      = 2.2     # Vertical rear wall thickness (ideal for snap-in USB-C tabs)

# ── RAZOR-SLIM FRONT BEZEL SPLIT & TONGUE-AND-GROOVE LIP ────────────────
FACE_T         = 2.2     # Solid front bezel wall thickness (mm)
SPLIT_D        = 2.6     # Ultra-slim front bezel depth (1.5mm bevel + 1.1mm rim -> killer 2-tone look!)
LIP_T          = 1.1     # Stepped alignment lip thickness (mm)
LIP_H          = 1.6     # Stepped alignment lip depth (mm)
LIP_CLR        = 0.25    # Radial clearance between front lip & rear groove (mm)

# ── M3 x 9mm SCREW STACK (9mm thread, 12mm total length) ────────────────
# Stack: 2.8mm rear pillar seat + 1.6mm PCB + 4.6mm thread engagement in front boss = 9.0mm!
M3_PILOT       = 2.75    # Self-tapping M3 pilot hole in front boss (PLA/PETG)
M3_CLEAR       = 3.4     # M3 clearance through-hole in rear pillar seat
M3_CBORE_D     = 6.6     # Counterbore tunnel for M3 screw head & screwdriver shaft
PILLAR_OUT_D   = 9.2     # Outer diameter of rear hollow screw pillars
PILLAR_SEAT_T  = 2.8     # Clamping wall thickness behind PCB for exact M3x9 fit

# ── SNAP-IN USB-C SOCKET & SPEAKER ──────────────────────────────────────
USB_CUT_W      = 14.0    # Measured snap-in USB-C cutout width (mm)
USB_CUT_H      = 5.5     # Measured snap-in USB-C cutout height (mm)
USB_CUT_R      = 0.8
USB_CENTER_Z   = 11.0    # Height of USB-C socket center above desk tabletop (mm)
SPK_DIA        = 24.4    # Internal speaker retaining ring inner diameter (8Ω 1W)

TILT_RAD       = math.radians(TILT_DEG)
SIN_T          = math.sin(TILT_RAD)
COS_T          = math.cos(TILT_RAD)
Z_TOP          = FRONT_H * COS_T          # ~54.72 mm


def rounded_rect_cs(w, h, r, segs=36):
    """2D rounded rectangle CrossSection centered at (0, 0)."""
    r = min(r, w / 2.0 - 0.05, h / 2.0 - 0.05)
    pts = []
    hw, hh = w / 2.0 - r, h / 2.0 - r
    corners = [(hw, hh, 0.0), (-hw, hh, math.pi / 2), (-hw, -hh, math.pi), (hw, -hh, 3 * math.pi / 2)]
    per_c = max(6, segs // 4)
    for cx, cy, a0 in corners:
        for i in range(per_c + 1):
            a = a0 + (math.pi / 2.0) * i / per_c
            pts.append([cx + r * math.cos(a), cy + r * math.sin(a)])
    return CrossSection([pts])


def front_to_desk(obj):
    """Front local (u=right, v=up along 34° face, w=outward normal) -> Desk (X, Y, Z)."""
    cy = (FRONT_H / 2.0) * SIN_T
    cz = (FRONT_H / 2.0) * COS_T
    return obj.transform([
        [1.0,    0.0,     0.0, 0.0],
        [0.0,  SIN_T,  -COS_T,  cy],
        [0.0,  COS_T,   SIN_T,  cz],
    ])


def desk_to_front(obj):
    """Inverse of front_to_desk: Desk (X, Y, Z) -> Front local (u, v, w)."""
    cy = (FRONT_H / 2.0) * SIN_T
    cz = (FRONT_H / 2.0) * COS_T
    ty = -(SIN_T * cy + COS_T * cz)
    tz = -(-COS_T * cy + SIN_T * cz)
    return obj.transform([
        [1.0,    0.0,   0.0, 0.0],
        [0.0,  SIN_T, COS_T,  ty],
        [0.0, -COS_T, SIN_T,  tz],
    ])


def rear_to_desk(obj):
    """Rear local (rx=right viewing back = -X, ry=up = +Z, rz=outward = +Y) -> Desk."""
    return obj.transform([
        [-1.0, 0.0, 0.0, 0.0],
        [ 0.0, 0.0, 1.0, DEPTH],
        [ 0.0, 1.0, 0.0, Z_TOP / 2.0],
    ])


def make_sloped_roof_clip(z_inset=0.0):
    """
    2D Y-Z profile extruded along X matching the EXACT lofted outer roof line
    from front bevel shoulder (y_f, z_f) to rear fillet shoulder (DEPTH - REAR_FILLET, z_rear),
    shifted down by z_inset so the internal cavity NEVER punctures the sloped roof or rear fillet!
    """
    c = FRONT_CHAMFER
    rf = REAR_FILLET
    y_tf = FRONT_H * SIN_T
    z_rear = Z_TOP - (DEPTH - y_tf) * math.tan(math.radians(TOP_SLOPE_DEG))

    # Exact outer loft roof endpoints
    y_f = y_tf + c * COS_T
    z_f = Z_TOP - c * SIN_T
    y_r = DEPTH - rf
    z_r = z_rear

    slope = (z_f - z_r) / (y_r - y_f)
    z_at_front_ext = z_f + (y_f - (-20.0)) * slope - z_inset
    z_at_y_r = z_r - z_inset
    z_at_rear_wall = z_r - (DEPTH - y_r) * slope - z_inset - 1.2  # Extra drop for R3.5mm rear-top fillet

    pts = [
        [-20.0, -10.0],
        [DEPTH + 10.0, -10.0],
        [DEPTH + 10.0, z_at_rear_wall],
        [DEPTH, z_at_rear_wall],
        [y_r, z_at_y_r],
        [-20.0, z_at_front_ext],
    ]
    prof = Manifold.extrude(CrossSection([pts]), OUTER_W + 20.0).translate([0.0, 0.0, -(OUTER_W + 20.0) / 2.0])
    return prof.transform([
        [0.0, 0.0, 1.0, 0.0],
        [1.0, 0.0, 0.0, 0.0],
        [0.0, 1.0, 0.0, 0.0],
    ])


def make_outer_wedge():
    """
    20° Braun/Apple lofted wedge (ZERO triangular crease at top-side corners!):
      - Single continuous batch_hull from 20° front rounded-rect (R7.0mm + crisp 1.6mm 45° bevel)
        to vertical rear rounded-rect (Z_REAR, R7.0mm + uniform smooth R3.5mm rear fillet).
      - Top-side R7.0mm corners flow seamlessly along the 15° sloped roof just like the bottom!
      - Clipped cleanly at Z >= 0 for a dead-flat horizontal desk base.
    """
    c = FRONT_CHAMFER
    rf = REAR_FILLET
    y_tf = FRONT_H * SIN_T
    z_rear = Z_TOP - (DEPTH - y_tf) * math.tan(math.radians(TOP_SLOPE_DEG))

    slices = []
    # 1. Front flat face (w = 0) and 45° bevel shoulder (w = -c)
    slices.append(front_to_desk(
        Manifold.extrude(rounded_rect_cs(OUTER_W - 2 * c, FRONT_H - 2 * c, CORNER_R - c * 0.6, segs=40), 0.04)
        .translate([0, 0, -0.04])
    ))
    slices.append(front_to_desk(
        Manifold.extrude(rounded_rect_cs(OUTER_W, FRONT_H, CORNER_R, segs=40), 0.04)
        .translate([0, 0, -c - 0.04])
    ))

    # 2. Rear vertical face (Y = DEPTH - rf .. DEPTH) with uniform smooth R3.5mm perimeter fillet
    #    Centered vertically at Z = z_rear / 2.0 so bottom sits on Z = 0 and top sits at Z = z_rear!
    steps = 8
    for i in range(steps + 1):
        ang = (math.pi / 2.0) * i / steps
        inset = rf * (1.0 - math.sin(ang))
        y_pos = DEPTH - rf * (1.0 - math.cos(ang))
        cr = max(1.0, CORNER_R - inset * 0.75)
        # Rear local (rx, ry, rz) -> Desk (X=-rx, Y=y_pos, Z=ry + z_rear/2)
        s = Manifold.extrude(rounded_rect_cs(OUTER_W - 2 * inset, z_rear - 2 * inset, cr, segs=40), 0.04)
        s = s.transform([
            [-1.0, 0.0, 0.0, 0.0],
            [ 0.0, 0.0, 1.0, y_pos - 0.04],
            [ 0.0, 1.0, 0.0, z_rear / 2.0],
        ])
        slices.append(s)

    loft = Manifold.batch_hull(slices)
    # Clip strictly to Z >= 0 so desk base is dead-flat horizontal
    desk_clip = Manifold.cube([OUTER_W + 20.0, DEPTH + 40.0, 120.0], center=True).translate([0.0, DEPTH / 2.0, 60.0])
    return loft ^ desk_clip


def make_common_cavity():
    """Shared internal cavity (fits shifted 87.9x50.7mm PCB + ESP32 + Speaker + USB-C) leaving >=2.2mm floor/rear walls."""
    inner_w = PCB_W + PCB_USB_EXTRA + 2 * TOL   # 87.9 mm
    inner_h = PCB_H + 2 * TOL                   # 50.7 mm
    pocket_cx = PCB_CX - PCB_USB_EXTRA / 2.0    # -4.0 mm (spans u in [-47.95, +39.95])
    front_pocket = front_to_desk(
        Manifold.extrude(rounded_rect_cs(inner_w, inner_h, 2.0, segs=28), 70.0)
        .translate([pocket_cx, 0.0, -FACE_T - 70.0])
    )
    inner_z_h = Z_TOP - WALL - BOTTOM_WALL
    inner_z_c = BOTTOM_WALL + inner_z_h / 2.0
    safe_inner_box = (
        Manifold.cube([OUTER_W - 2 * WALL, DEPTH - REAR_WALL + 35.0, inner_z_h], center=True)
        .translate([0.0, (DEPTH - REAR_WALL - 35.0) / 2.0, inner_z_c])
    ) ^ make_sloped_roof_clip(z_inset=WALL + 0.4)
    rear_bay = (
        Manifold.cube([OUTER_W - 2 * WALL, DEPTH - REAR_WALL - 10.0, inner_z_h], center=True)
        .translate([0.0, (DEPTH - REAR_WALL + 10.0) / 2.0, inner_z_c])
    )
    front_guard = front_to_desk(
        Manifold.cube([OUTER_W + 20, FRONT_H + 20, 40.0], center=True)
        .translate([0, 0, 20.0 - (FACE_T + 1.5)])
    )
    return (front_pocket ^ safe_inner_box) + ((rear_bay ^ safe_inner_box) - front_guard)


def build_front_cap():
    """
    PART 1: Seamless Full-Width Front Cap (zero seam on front face, matching concept render!)
    Returned laid FLAT face-down on Z = 0 for 100% support-free FDM printing.
    """
    outer = make_outer_wedge()
    cavity = make_common_cavity()

    # Keep front half: w in [-SPLIT_D, +10]
    front_half_box = front_to_desk(
        Manifold.cube([OUTER_W + 20, FRONT_H + 20, SPLIT_D + 10.0], center=True)
        .translate([0, 0, 5.0 - SPLIT_D / 2.0])
    )
    cap = (outer ^ front_half_box) - cavity

    # Add stepped tongue alignment lip extending backward from w = -SPLIT_D to -(SPLIT_D + LIP_H)
    lip_outer = Manifold.extrude(
        rounded_rect_cs(OUTER_W - 2 * (WALL - LIP_T), FRONT_H - 2 * (WALL - LIP_T), max(1.5, CORNER_R - 1.5), segs=32),
        LIP_H + 0.4
    ).translate([0, 0, -SPLIT_D - LIP_H])
    lip_inner = Manifold.extrude(
        rounded_rect_cs(OUTER_W - 2 * WALL, FRONT_H - 2 * WALL, max(1.0, CORNER_R - 2.6), segs=32),
        LIP_H + 2.0
    ).translate([0, 0, -SPLIT_D - LIP_H - 0.8])
    lip_desk = front_to_desk(lip_outer - lip_inner) ^ outer
    # Keep lip strictly above bottom floor Z = BOTTOM_WALL so it doesn't poke through desk base
    floor_guard = Manifold.cube([OUTER_W + 20, DEPTH + 20, Z_TOP], center=True).translate([0, DEPTH / 2.0, BOTTOM_WALL + Z_TOP / 2.0])
    cap = cap + ((lip_desk ^ floor_guard) - cavity)

    # Convert to front-local coords (u=right, v=up, w=outward, front at w=0) for clean front cutouts & M3 bosses
    cap_local = desk_to_front(cap)

    # 1. Exact 60.0 x 44.5 mm display window + 45° front chamfer (aligned to exact WIN_CX = +4.0mm!)
    win_cut = Manifold.extrude(rounded_rect_cs(WIN_W, WIN_H, WIN_R, segs=32), FACE_T + 6.0).translate([WIN_CX, 0.0, -FACE_T - 3.0])
    win_chamfer = Manifold.batch_hull([
        Manifold.extrude(rounded_rect_cs(WIN_W + 2.0, WIN_H + 2.0, WIN_R + 0.7, segs=32), 0.1).translate([WIN_CX, 0.0, 0.05]),
        Manifold.extrude(rounded_rect_cs(WIN_W, WIN_H, WIN_R, segs=32), 0.1).translate([WIN_CX, 0.0, -1.05]),
    ])

    # 2. Exact LDR pinhole on RIGHT bezel (4mm from right edge of 86mm PCB, 11mm from top edge of 50mm PCB)
    ldr_u = PCB_CX + (PCB_W / 2.0 - LDR_FROM_RIGHT)   # +39.6 mm
    ldr_v = (PCB_H / 2.0 - LDR_FROM_TOP)              # +14.0 mm
    ldr_hole = Manifold.cylinder(FACE_T + 6.0, LDR_DIA / 2.0, LDR_DIA / 2.0, 32).translate([ldr_u, ldr_v, -FACE_T - 3.0])
    ldr_cone = Manifold.cylinder(1.2, LDR_DIA / 2.0, LDR_DIA / 2.0 + 0.9, 32).translate([ldr_u, ldr_v, -1.15])

    cap_local = cap_local - win_cut - win_chamfer - ldr_hole - ldr_cone

    # 3. 4x M3 Self-Tapping Bosses at exact 78x42mm PCB holes (height = GLASS_T = 3.8mm)
    #    Pilot hole 4.8mm deep (w = -6.0 to -1.2), leaving 1.2mm solid front face!
    for sx in (-1, 1):
        for sy in (-1, 1):
            su = PCB_CX + sx * (HOLE_DX / 2.0)
            sv = sy * (HOLE_DY / 2.0)
            boss = Manifold.cylinder(GLASS_T + 0.6, 3.2, 3.2, 28).translate([su, sv, -FACE_T - GLASS_T])
            pilot = Manifold.cylinder(GLASS_T + 1.4, M3_PILOT / 2.0, M3_PILOT / 2.0, 24).translate([su, sv, -FACE_T - GLASS_T - 0.4])
            cap_local = cap_local + boss - pilot

    # Flip face-down onto Z = 0 for 100% support-free flat printing:
    # Right-handed rotation 180° around Y axis: (u, v, w) -> (-u, v, -w)
    cap_print = cap_local.transform([
        [-1.0, 0.0,  0.0, 0.0],
        [ 0.0, 1.0,  0.0, 0.0],
        [ 0.0, 0.0, -1.0, 0.0],
    ])
    return cap_print


def build_rear_wedge():
    """
    PART 2: 34° Wedge Rear Body with 4x M3x9 Hollow Screw Pillars
    Stands upright on flat desk base Z = 0 (prints 100% support-free!).
    """
    outer = make_outer_wedge()
    cavity = make_common_cavity()

    # Keep rear half: w < -SPLIT_D
    rear_half_box = front_to_desk(
        Manifold.cube([OUTER_W + 20, FRONT_H + 20, 90.0], center=True)
        .translate([0, 0, -SPLIT_D - 45.0])
    )
    body = (outer ^ rear_half_box) - cavity

    # Subtract matching groove for front cap's stepped alignment lip (with radial clearance LIP_CLR)
    groove_w = OUTER_W - 2 * (WALL - LIP_T - LIP_CLR)
    groove_h = FRONT_H - 2 * (WALL - LIP_T - LIP_CLR)
    groove = front_to_desk(
        Manifold.extrude(rounded_rect_cs(groove_w, groove_h, max(1.5, CORNER_R - 1.3), segs=32), LIP_H + 1.0)
        .translate([0, 0, -SPLIT_D - LIP_H - 0.3])
    )
    floor_guard = Manifold.cube([OUTER_W + 20, DEPTH + 20, Z_TOP], center=True).translate([0, DEPTH / 2.0, BOTTOM_WALL + Z_TOP / 2.0])
    body = body - (groove ^ floor_guard)

    # 3. 4x INTERNAL HOLLOW M3x9 SCREW PILLARS (pendlerbuddy style! 78x42mm pitch, perpendicular to 34° face!)
    #    Front clamping tip sits at w_pcb_back = -(FACE_T + GLASS_T + PCB_T) = -7.6 mm
    #    (protrudes 1.0mm past split plane w=-8.6mm to clamp directly against PCB back!)
    w_pcb_back = -(FACE_T + GLASS_T + PCB_T)   # -7.6 mm
    pillar_len = 52.0

    for sx in (-1, 1):
        for sy in (-1, 1):
            su = PCB_CX + sx * (HOLE_DX / 2.0)
            sv = sy * (HOLE_DY / 2.0)
            pillar = front_to_desk(
                Manifold.cylinder(pillar_len, PILLAR_OUT_D / 2.0, PILLAR_OUT_D / 2.0, 28)
                .translate([su, sv, w_pcb_back - pillar_len])
            )
            body = body + (pillar ^ outer)

    # Subtract exact M3x9 clearance hole + Ø6.6mm counterbore tunnel through each pillar!
    for sx in (-1, 1):
        for sy in (-1, 1):
            su = PCB_CX + sx * (HOLE_DX / 2.0)
            sv = sy * (HOLE_DY / 2.0)
            m3_thru = front_to_desk(
                Manifold.cylinder(pillar_len + 10.0, M3_CLEAR / 2.0, M3_CLEAR / 2.0, 24)
                .translate([su, sv, w_pcb_back - pillar_len - 2.0])
            )
            cbore = front_to_desk(
                Manifold.cylinder(pillar_len + 10.0, M3_CBORE_D / 2.0, M3_CBORE_D / 2.0, 28)
                .translate([su, sv, w_pcb_back - PILLAR_SEAT_T - (pillar_len + 10.0)])
            )
            body = body - m3_thru - cbore

    # 4. SNAP-IN 2-PIN PANEL-MOUNT USB-C CUTOUT (14.0 x 5.5 mm, centered on lower rear wall)
    usb_ry = USB_CENTER_Z - Z_TOP / 2.0
    usb_cut = rear_to_desk(
        Manifold.extrude(rounded_rect_cs(USB_CUT_W, USB_CUT_H, USB_CUT_R, segs=24), REAR_WALL + 6.0)
        .translate([0.0, usb_ry, -REAR_WALL - 3.0])
    )
    body = body - usb_cut

    # 5. PRECISION CIRCULAR SPEAKER GRILLE + RETAINING RING (RIGHT rear, clear of top M3 tunnels!)
    spk_rx, spk_ry = 14.5, 2.0
    grille_cuts = [Manifold.cylinder(REAR_WALL + 6.0, 1.0, 1.0, 16).translate([spk_rx, spk_ry, -REAR_WALL - 3.0])]
    for ring_idx in range(1, 4):
        rad = ring_idx * 3.3
        cnt = ring_idx * 6
        for i in range(cnt):
            ang = 2.0 * math.pi * i / cnt
            grille_cuts.append(
                Manifold.cylinder(REAR_WALL + 6.0, 0.9, 0.9, 16)
                .translate([spk_rx + rad * math.cos(ang), spk_ry + rad * math.sin(ang), -REAR_WALL - 3.0])
            )
    for gc in grille_cuts:
        body = body - rear_to_desk(gc)

    spk_ring = rear_to_desk(
        (Manifold.cylinder(4.8, SPK_DIA / 2.0 + 1.6, SPK_DIA / 2.0 + 1.6, 40)
         - Manifold.cylinder(5.6, SPK_DIA / 2.0, SPK_DIA / 2.0, 40).translate([0, 0, -0.4])
         - Manifold.cube([7.0, 8.0, 6.0], center=True).translate([0, -SPK_DIA / 2.0 - 1.0, 2.0]))
        .translate([spk_rx, spk_ry, -REAR_WALL - 4.2])
    )
    body = body + (spk_ring ^ outer)

    # 6. FOUR HORIZONTAL PILL VENTILATION SLOTS (LEFT rear behind ESP32, clear of top M3 tunnels!)
    for i in range(4):
        vy = -2.5 + i * 3.6
        vent = rear_to_desk(
            Manifold.extrude(rounded_rect_cs(20.0, 1.7, 0.8, segs=16), REAR_WALL + 6.0)
            .translate([-15.5, vy, -REAR_WALL - 3.0])
        )
        body = body - vent

    # 7. FOUR BOTTOM SILICONE FOOT RECESSES (Ø7.0mm x 0.8mm deep, clear of bottom M3 tunnels!)
    for sx in (-1, 1):
        for fy in (12.0, DEPTH - 6.5):
            body = body - Manifold.cylinder(1.2, 3.5, 3.5, 24).translate([sx * 24.0, fy, -0.4])

    return body


def write_manifold_stl(manifold, path, header_text):
    mesh = manifold.to_mesh()
    verts = mesh.vert_properties[:, :3]
    tris = mesh.tri_verts
    z_min = float(verts[:, 2].min())
    verts = verts.copy()
    verts[:, 2] -= z_min

    os.makedirs(os.path.dirname(path), exist_ok=True)
    hdr = header_text.encode("ascii", errors="ignore")[:80].ljust(80, b"\x00")
    with open(path, "wb") as f:
        f.write(hdr)
        f.write(struct.pack("<I", len(tris)))
        for i0, i1, i2 in tris:
            a, b, c = verts[i0], verts[i1], verts[i2]
            n = np.cross(b - a, c - a)
            nl = np.linalg.norm(n)
            if nl > 0:
                n = n / nl
            f.write(struct.pack(
                "<12fH",
                float(n[0]), float(n[1]), float(n[2]),
                float(a[0]), float(a[1]), float(a[2]),
                float(b[0]), float(b[1]), float(b[2]),
                float(c[0]), float(c[1]), float(c[2]),
                0
            ))
    mn = verts.min(axis=0)
    mx = verts.max(axis=0)
    sz = mx - mn
    return len(tris), sz, manifold.status()


def main():
    out_dir = os.path.dirname(os.path.abspath(__file__))

    front = build_front_cap()
    front_path = os.path.join(out_dir, "smartdesk_v2_front.stl")
    n_f, sz_f, st_f = write_manifold_stl(front, front_path, "CYD SmartDesk V2 V8 Seamless Front Cap (M3x9)")
    print(f"[OK] {front_path}")
    print(f"     Status: {st_f} | Components: {len(front.decompose())} | Triangles: {n_f:,} | Size: {sz_f[0]:.1f} x {sz_f[1]:.1f} x {sz_f[2]:.1f} mm")

    rear = build_rear_wedge()
    rear_path = os.path.join(out_dir, "smartdesk_v2_rear.stl")
    n_r, sz_r, st_r = write_manifold_stl(rear, rear_path, "CYD SmartDesk V2 V8 34deg Rear Wedge (M3x9)")
    print(f"[OK] {rear_path}")
    print(f"     Status: {st_r} | Components: {len(rear.decompose())} | Triangles: {n_r:,} | Size: {sz_r[0]:.1f} x {sz_r[1]:.1f} x {sz_r[2]:.1f} mm")


if __name__ == "__main__":
    main()
