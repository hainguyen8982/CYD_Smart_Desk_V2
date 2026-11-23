// =========================================================================
//  CYD SmartDesk V2 — Minimalist 18° Desk Stand Enclosure
//  Target Hardware : NiceMCU-32S-DEV 2.8" IPS (86x50mm PCB, 87mm w/ USB-C)
//  Design Style    : Braun / Apple Minimalist Wedge (2-Piece, Support-Free)
//  Features        :
//    - 18° ergonomic viewing & capacitive touch angle
//    - Front chamfered display window + exact LDR pinhole (right bezel)
//    - Rear micro-perforated 8Ω 1W speaker grille (right side, away from Wi-Fi)
//    - Rear-bottom hidden USB-C cable routing channel (use 90° USB-C cable)
//    - Passive ventilation slots behind ESP32 module
//    - Clean backplate (RST/BOOT cutouts omitted per design spec)
//    - 4x M2.5 internal PCB standoffs (78x42mm pitch) + 4x M2.5 backplate screws
// =========================================================================

$fn = 48;

// ── PRINT PART SELECTOR ──────────────────────────────────────────────────
// "both"       = preview assembled (in-place)
// "print_plate"= both parts laid flat ready for slicing
// "body"       = Part 1: Main Wedge Body (print face-down, 0 supports)
// "backplate"  = Part 2: Rear Cover Plate (print flat, 0 supports)
part = "print_plate";

// ── MEASURED PCB PARAMETERS (NiceMCU-32S 2.8" IPS) ───────────────────────
pcb_w           = 86.0;    // PCB width (mm)
pcb_usb_extra   = 1.2;     // USB-C overhang on left side of front view
pcb_h           = 50.0;    // PCB height (mm)
pcb_t           = 1.6;     // PCB thickness (mm)
glass_t         = 3.8;     // LCD + capacitive glass thickness above PCB
hole_dx         = 78.0;    // Mounting hole horizontal pitch (mm)
hole_dy         = 42.0;    // Mounting hole vertical pitch (mm)
tol             = 0.35;    // FDM 3D print clearance (mm)

// ── DISPLAY WINDOW & LDR SENSOR (Front View Coordinates) ─────────────────
win_w           = 64.0;    // Visible glass bezel opening width (covers 57.6mm AA)
win_h           = 45.0;    // Visible glass bezel opening height (covers 43.2mm AA)
win_offset_x    = 0.0;     // Shift window left/right if needed
ldr_from_right  = 4.0;     // LDR center distance from right edge of 86mm PCB
ldr_from_top    = 11.0;    // LDR center distance from top edge of 50mm PCB
ldr_dia         = 3.0;     // LDR pinhole diameter (mm)

// ── ENCLOSURE OUTER GEOMETRY ─────────────────────────────────────────────
wall            = 2.6;     // Outer wall thickness (mm)
front_wall      = 2.2;     // Front bezel thickness (mm)
tilt_deg        = 18.0;    // Ergonomic backward tilt angle (degrees)
top_depth       = 17.0;    // Depth behind front face at top edge (mm)
corner_r        = 3.5;     // Outer rounded corner radius (mm)

// ── SPEAKER & USB-C ROUTING ──────────────────────────────────────────────
spk_dia         = 24.0;    // Speaker grille outer diameter (fits 20-28mm 8Ω 1W)
spk_offset_x    = 17.0;    // Placed on RIGHT side of rear (away from ESP32 Wi-Fi)
spk_offset_y    = 2.0;
usb_slot_w      = 10.0;    // Rear-bottom braided USB-C cable exit width
usb_slot_h      = 6.5;     // Rear-bottom cable exit height

// ── BACKPLATE & SCREWS ───────────────────────────────────────────────────
back_t          = 2.4;     // Backplate thickness (mm)
lip_depth       = 2.4;     // Stepped recess depth for backplate
m25_pilot       = 2.2;     // Self-tapping M2.5 pilot hole diameter
m25_clear       = 2.8;     // M2.5 clearance hole diameter
m25_head_d      = 5.2;     // M2.5 countersunk head diameter

// ── DERIVED DIMENSIONS ───────────────────────────────────────────────────
inner_w         = pcb_w + pcb_usb_extra + 2 * tol; // 87.9 mm
inner_h         = pcb_h + 2 * tol;                 // 50.7 mm
outer_w         = inner_w + 2 * wall;              // 93.1 mm
outer_h         = inner_h + 2 * wall;              // 55.9 mm
// Extra depth at bottom due to 18° tilt so the base sits flat on desk
extra_bot_d     = outer_h * sin(tilt_deg);         // ~17.3 mm
base_depth      = top_depth + extra_bot_d;         // ~34.3 mm

// ── HELPER MODULES ───────────────────────────────────────────────────────
module rounded_rect_2d(w, h, r) {
    offset(r = r)
        square([w - 2 * r, h - 2 * r], center = true);
}

// Wedge outer profile in the Y-Z plane (front face lies on Z=0, Y in [-outer_h/2, +outer_h/2])
// When standing on desk, the bottom bevel plane sits flat on the table at tilt_deg!
module wedge_solid(w, h, r, d_top, tilt) {
    // Build a tall rounded prism along +Z, then slice the bottom and back cleanly
    // so the base is flat at angle `tilt` and the rear face is vertical when standing!
    d_bot = d_top + h * sin(tilt);
    max_d = d_bot + 10;

    intersection() {
        // 1. Extruded rounded rectangle along +Z (front face at Z=0)
        linear_extrude(height = max_d)
            rounded_rect_2d(w, h, r);

        // 2. Side profile polygon in Y-Z plane extruded along X
        //    Front face: Z = 0, Y from -h/2 (bottom) to +h/2 (top)
        //    Bottom desk plane: starts at (Y=-h/2, Z=0), goes along direction (sin(tilt), cos(tilt))?
        //    Wait: when tilted backward by `tilt`, the desk normal in front-face coords is:
        //      n_desk = (0, cos(tilt), sin(tilt))  [pointing UP from desk]
        //    So the desk plane passing through front-bottom edge (Y=-h/2, Z=0) satisfies:
        //      (y + h/2)*cos(tilt) + z*sin(tilt) >= 0
        //    And the vertical rear wall has normal (0, -sin(tilt), cos(tilt)):
        //      Rear wall passes through top-rear corner (Y=+h/2, Z=d_top):
        //      -(y - h/2)*sin(tilt) + (z - d_top)*cos(tilt) <= 0
        rotate([0, 90, 0])
        linear_extrude(height = w + 2, center = true)
            polygon(points = [
                // In rotate([0,90,0]) coords: local X = -Z, local Y = Y
                [ 0,          -h/2 ],                          // Front-bottom
                [ 0,           h/2 ],                          // Front-top
                [ -d_top,      h/2 ],                          // Rear-top
                [ -(d_top + h * tan(tilt)), -h/2 - (d_top + h * tan(tilt)) * tan(tilt) ] // Extended rear-bottom
            ]);

        // 3. Flat desk bottom plane cut: (y + h/2)*cos(tilt) + z*sin(tilt) >= 0
        translate([0, -h/2, 0])
        rotate([tilt, 0, 0])
        translate([-w/2 - 2, 0, -5])
            cube([w + 4, h + 30, max_d + 20]);
    }
}

// =========================================================================
//  PART 1: MAIN WEDGE BODY (Front face at Z = 0, prints face-down!)
// =========================================================================
module smartdesk_body() {
    // PCB center is shifted slightly right by +pcb_usb_extra/2 to leave USB-C clearance on left
    pcb_cx = pcb_usb_extra / 2;

    // LDR position in front-face coordinates (X right, Y up)
    ldr_x = pcb_cx + (pcb_w / 2 - ldr_from_right);
    ldr_y = (pcb_h / 2 - ldr_from_top);

    // Rear wall plane geometry (in front-face coordinates)
    // Top-rear point is at (0, +outer_h/2, top_depth)
    // Rear wall normal points along (0, -sin(tilt_deg), cos(tilt_deg))
    rear_h = outer_h * cos(tilt_deg) - top_depth * sin(tilt_deg);

    difference() {
        union() {
            // Outer minimalist wedge shell
            wedge_solid(outer_w, outer_h, corner_r, top_depth, tilt_deg);
        }

        // 1. Hollow internal cavity (leaves `front_wall` on front and `wall` on sides)
        translate([0, 0, front_wall])
            wedge_solid(inner_w, inner_h, max(1.0, corner_r - wall), top_depth + 20, tilt_deg);

        // 2. Front display viewing window (clean + 45° inner chamfer for capacitive touch)
        translate([pcb_cx + win_offset_x, 0, -0.5])
            linear_extrude(height = front_wall + 1.5)
                rounded_rect_2d(win_w, win_h, 1.5);

        // 45° chamfer on front face of display window
        hull() {
            translate([pcb_cx + win_offset_x, 0, -0.1])
                linear_extrude(height = 0.2)
                    rounded_rect_2d(win_w + 2.4, win_h + 2.4, 2.2);
            translate([pcb_cx + win_offset_x, 0, 1.2])
                linear_extrude(height = 0.2)
                    rounded_rect_2d(win_w, win_h, 1.5);
        }

        // 3. LDR Ambient Light Sensor pinhole on right vertical bezel (with chamfered light funnel)
        translate([ldr_x, ldr_y, -0.5])
            cylinder(d = ldr_dia, h = front_wall + 2.0);
        translate([ldr_x, ldr_y, -0.1])
            cylinder(d1 = ldr_dia + 2.0, d2 = ldr_dia, h = 1.2);

        // 4. Stepped rear recess for backplate (depth = lip_depth from the rear vertical wall)
        translate([0, outer_h/2, top_depth])
        rotate([-tilt_deg, 0, 0])
        translate([0, -outer_h/2, -lip_depth])
            linear_extrude(height = lip_depth + 6)
                rounded_rect_2d(outer_w - 2 * (wall - 1.2), outer_h + 10, max(1.0, corner_r - 1.2));

        // 5. Rear-bottom USB-C cable exit notch (centered at bottom of rear face)
        translate([0, -outer_h/2, 0])
        rotate([tilt_deg, 0, 0])
        translate([-usb_slot_w/2, -1, base_depth - 14])
            cube([usb_slot_w, usb_slot_h + 1, 20]);
    }

    // ── INTERNAL STRUCTURAL FEATURES (ADDED INSIDE CAVITY) ───────────────
    // 4x PCB Mounting Standoffs (78x42mm pitch) resting behind the glass
    for (sx = [-1, 1], sy = [-1, 1]) {
        translate([pcb_cx + sx * hole_dx / 2, sy * hole_dy / 2, front_wall])
        difference() {
            cylinder(d = 5.4, h = glass_t);
            translate([0, 0, 0.6])
                cylinder(d = m25_pilot, h = glass_t + 1);
        }
    }

    // 4x Backplate Corner Screw Bosses (anchored to inner walls)
    intersection() {
        wedge_solid(outer_w - 0.4, outer_h - 0.4, corner_r, top_depth - lip_depth, tilt_deg);
        union() {
            for (sx = [-1, 1], sy = [-1, 1]) {
                translate([sx * (inner_w/2 - 3.2), sy * (inner_h/2 - 3.2), front_wall + glass_t + pcb_t + 1.0])
                difference() {
                    cylinder(d = 6.2, h = base_depth);
                    cylinder(d = m25_pilot, h = base_depth + 1);
                }
            }
        }
    }
}

// =========================================================================
//  PART 2: REAR BACKPLATE (Laid flat at Z = 0, prints 100% support-free!)
// =========================================================================
module smartdesk_backplate() {
    bp_w = outer_w - 2 * (wall - 1.2) - 2 * tol;
    // Height of the vertical rear face along the tilted rear plane
    bp_h = (outer_h / cos(tilt_deg)) - (top_depth * tan(tilt_deg)) - 2 * (wall - 1.2) - 2 * tol;
    bp_r = max(1.0, corner_r - 1.4);

    difference() {
        union() {
            // Flat outer backplate body
            linear_extrude(height = back_t)
                rounded_rect_2d(bp_w, bp_h, bp_r);

            // Internal speaker retaining ring on RIGHT side (fits 20-28mm 8Ω 1W mini speaker)
            translate([spk_offset_x, spk_offset_y, back_t])
            difference() {
                cylinder(d = spk_dia + 3.0, h = 4.0);
                cylinder(d = spk_dia + 0.4, h = 4.5);
                // Wire exit notch toward bottom SPEAKER JST header
                translate([-3, -spk_dia/2 - 3, -0.5])
                    cube([6, 6, 5.5]);
            }
        }

        // 1. Precision micro-perforated circular speaker grille (RIGHT side, away from ESP32 Wi-Fi)
        translate([spk_offset_x, spk_offset_y, -0.5]) {
            cylinder(d = 2.0, h = back_t + 1.5);
            for (ring = [1 : 3]) {
                r_ring = ring * 3.4;
                n_holes = ring * 6;
                for (i = [0 : n_holes - 1]) {
                    ang = i * 360 / n_holes;
                    translate([r_ring * cos(ang), r_ring * sin(ang), 0])
                        cylinder(d = 1.8, h = back_t + 1.5);
                }
            }
        }

        // 2. Passive ventilation slots on UPPER-LEFT (behind ESP32 module, zero Wi-Fi blocking)
        for (i = [0 : 3]) {
            translate([-22, 6 + i * 3.6, -0.5])
                linear_extrude(height = back_t + 1.5)
                    rounded_rect_2d(22, 1.6, 0.7);
        }

        // 3. Rear-bottom USB-C cable exit notch
        translate([-usb_slot_w / 2, -bp_h / 2 - 1, -0.5])
            cube([usb_slot_w, usb_slot_h + 1, back_t + 1.5]);

        // 4. 4x Countersunk M2.5 screw holes at corners
        for (sx = [-1, 1], sy = [-1, 1]) {
            translate([sx * (bp_w / 2 - 4.0), sy * (bp_h / 2 - 4.0), -0.5]) {
                cylinder(d = m25_clear, h = back_t + 2);
                cylinder(d1 = m25_head_d, d2 = m25_clear, h = 1.6);
            }
        }
    }
}

// ── RENDER DISPATCH ──────────────────────────────────────────────────────
if (part == "body") {
    smartdesk_body();
} else if (part == "backplate") {
    smartdesk_backplate();
} else if (part == "print_plate") {
    // Both parts laid flat on Z=0 build plate side-by-side ready to slice
    translate([0, 34, 0]) smartdesk_body();
    translate([0, -34, 0]) smartdesk_backplate();
} else {
    // Assembled preview
    color([0.15, 0.16, 0.18]) smartdesk_body();
    color([0.22, 0.24, 0.27])
        translate([0, outer_h/2, top_depth])
        rotate([-tilt_deg, 0, 0])
        translate([0, -outer_h/2, -lip_depth])
            smartdesk_backplate();
}
