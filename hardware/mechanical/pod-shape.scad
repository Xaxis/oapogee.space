// Form B: the external pod. Geometry shared by the base and the cap.
//
// The pod straps to the outside of a body tube and comes off again. Nothing is
// drilled, cut or glued to a rocket you care about, which is the argument for
// it: it is the form factor that fits a rocket you already own. What it costs
// is on the Mounting page and is not hidden. A pod adds drag, adds asymmetric
// drag, and moves both the centre of gravity and the centre of pressure.
// Re-simulate before you fly it.
//
// WHY IT IS TWO PARTS
//
// A single-piece pod has a cavity that opens somewhere, and every choice of
// where is bad. Opening upward leaves the payload uncovered. Opening downward
// onto the tube leaves nothing to hold the board. And neither prints: the pod
// is convex on top and concave underneath, so no orientation puts a flat face
// on the bed.
//
// Split at the top of the cell, both halves have one large flat face, both
// print that face down with no supports, and the saddle ends up facing upward
// where a concave surface is free rather than an overhang.

include <common.scad>

// --- the cell and the cavity it sits in --------------------------------------

cell_l = cell_length + 2 * cell_squeeze;
cell_w = cell_width + 2 * cell_squeeze;
cell_h = cell_thickness + 2 * cell_squeeze;

// Long enough for the board plus a boss pad at each end, wide enough for
// whichever of the board and the cell is wider.
cavity_l = board_length + 2 * fit_clearance + 2 * pod_boss_pad;
cavity_w = max(board_width + 2 * fit_clearance, cell_w);
cavity_h = cell_h + pod_cavity_gap + comp_height_bottom + board_thickness + comp_height_top;

// --- stacking up from the tube -----------------------------------------------

saddle_r = (tube_od + fit_clearance) / 2;
// The tube axis sits below the pod so the tube intrudes pod_saddle_depth into
// the underside. Everything else is stacked from there.
saddle_z = -(saddle_r - pod_saddle_depth);

cavity_z = pod_saddle_depth + floor_thickness;
// The joint: the top of the cell. Below it the base carries the cell and takes
// the straps; above it the cap covers the board.
split_z = cavity_z + cell_h + pod_cavity_gap;

pod_l = pod_nose_len + cavity_l + 2 * pod_wall + pod_tail_len;
pod_w = cavity_w + 2 * pod_wall;
pod_h = cavity_z + cavity_h + pod_wall;

body_x0 = pod_nose_len;
body_x1 = pod_l - pod_tail_len;
cav_x0 = body_x0 + pod_wall;

// The tips do not taper to nothing in height, only in width. A tip shallow
// enough to be elegant is a tip the saddle and the strap slot between them cut
// clean through, which leaves the nose as a separate loose object.
tip_h = pod_saddle_depth + strap_slot_height + 3 * pod_wall;
corner_r = min(pod_wall * 1.5, pod_w * 0.25 / 2, tip_h / 2);

slot_z = pod_saddle_depth + pod_wall;
slot_x = [pod_nose_len * 0.6, pod_l - pod_tail_len * 0.6];

boss_x = [cav_x0 + pod_boss_pad / 2, cav_x0 + cavity_l - pod_boss_pad / 2];
boss_d = mount_hole_dia + 2.4;

assert(tip_h < pod_h, "tip_h must be shorter than the pod is tall");
assert(split_z < cavity_z + cavity_h, "the split must fall inside the cavity");

// --- shapes ------------------------------------------------------------------

module pod_outer() {
    hull() {
        // Small but not zero. A hull to a knife edge prints as a ragged single
        // extrusion that snaps off in a field.
        station(0, pod_w * 0.25, tip_h, corner_r);
        station(body_x0, pod_w, pod_h, corner_r);
        station(body_x1, pod_w, pod_h, corner_r);
        station(pod_l, pod_w * 0.3, tip_h, corner_r);
    }
}

module saddle() {
    translate([-1, 0, saddle_z])
        rotate([0, 90, 0])
            cylinder(r = saddle_r, h = pod_l + 2);
}

// Full height, then clipped by whichever half is being built. No overlap fudge
// on the top face: the base's cavity is opened by the split cut below it, and a
// +1 here came straight out of the cap roof, leaving 0.6 mm where pod_wall
// promises 1.6 mm.
module cavity(grow = 0) {
    translate([cav_x0 - grow, -cavity_w / 2 - grow, cavity_z])
        cube([cavity_l + 2 * grow, cavity_w + 2 * grow, cavity_h]);
}

// Two slots, fore and aft, so the pod is held at both ends and cannot pivot
// about a single strap. They pass through the solid tips, clear of the cavity,
// and sit just above the saddle so the strap wraps the tube closely.
module strap_slots() {
    for (x = slot_x)
        translate([x - strap_slot_width / 2, -pod_w, slot_z])
            cube([strap_slot_width, pod_w * 2, strap_slot_height]);
}

module bosses() {
    for (x = boss_x) translate([x, 0, cavity_z]) cylinder(d = boss_d, h = split_z - cavity_z);
}

module boss_pilots() {
    // Undersized so an M2 cuts its own thread in the plastic. No insert needed.
    // Blind: the bottom stops at the cavity floor, leaving floor_thickness
    // between the screw tip and the saddle. Starting 1 mm lower broke through
    // into the surface that sits on the body tube, with 0.2 mm of skin left.
    for (x = boss_x)
        translate([x, 0, cavity_z]) cylinder(d = mount_hole_dia - 0.5, h = pod_lip_height + split_z - cavity_z);
}

// The other half of each screw joint. Without these the screws pass through the
// cap roof and then 8.1 mm of open air before reaching the base bosses, so the
// two halves cannot be clamped together at all.
module cap_bosses() {
    for (x = boss_x)
        translate([x, 0, split_z])
            cylinder(d = boss_d, h = cavity_z + cavity_h - split_z);
}

module cap_screw_holes() {
    for (x = boss_x) translate([x, 0, split_z - 1]) cylinder(d = mount_hole_dia, h = pod_h);
}

// A spigot: the base wall continues up inside the cap so the halves locate on
// each other rather than on the screws. Screws through an unaligned joint bend
// the cap instead of closing it.
module lip() {
    t = pod_wall * 0.6;
    // Flush with the cavity wall, so the spigot fuses to the top of the base
    // wall it continues. Insetting it by a print clearance instead left its
    // whole underside hanging over cavity void, and the base exported as two
    // disconnected solids: a pod, and a loose rectangular ring. The cap gets
    // its clearance by growing its own cavity, not by shrinking this.
    translate([cav_x0 + cavity_l / 2, 0, split_z])
        linear_extrude(pod_lip_height)
            difference() {
                square([cavity_l, cavity_w], center = true);
                square([cavity_l - 2 * t, cavity_w - 2 * t], center = true);
            }
}

// Ports on both side walls of the cap, level with the board. Symmetric left to
// right so yaw averages out, which is the rule the Mounting page states. They
// are dimples, not holes: see port_mark in common.scad for why.
module port_marks() {
    depth = pod_wall * 0.6;
    per_side = max(1, floor(port_count / 2));
    span = (board_length + 2 * fit_clearance) * 0.5;
    x0 = cav_x0 + cavity_l / 2 - span / 2;
    step = per_side > 1 ? span / (per_side - 1) : 0;
    z = (split_z + cavity_z + cavity_h) / 2;

    for (i = [0 : per_side - 1])
        for (s = [-1, 1])
            translate([x0 + i * step, s * pod_w / 2, z])
                rotate([0, 0, s > 0 ? -90 : 90])
                    port_mark(depth);
}

module below_split() {
    translate([-1, -pod_w, -pod_h]) cube([pod_l + 2, pod_w * 2, pod_h + split_z]);
}

module above_split() {
    translate([-1, -pod_w, split_z]) cube([pod_l + 2, pod_w * 2, pod_h]);
}
