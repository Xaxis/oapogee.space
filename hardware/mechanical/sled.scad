// Form A: the internal sled.
//
// Slides into a payload bay or a coupler. Lighter than the pod, no drag penalty
// at all, and it puts the payload on the centreline where its mass has the
// least awkward effect on stability. Its limitation is that it needs a rocket
// with somewhere to put it.
//
//   openscad -o oapogee-sled.stl sled.scad
//   openscad -D tube_id=40.5 -o sled-bt60.stl sled.scad
//
// Print floor down, no supports. Print one and check it slides in your own tube
// before printing anything else: paper tube bore varies enough between
// production runs that this is worth five minutes.
//
// The sled has no static ports. It does not enclose the sensor, the airframe
// does, so the ports go in the body tube and the Mounting page covers where.

include <common.scad>

// --- derived geometry -------------------------------------------------------

bore_r = (tube_id - fit_clearance) / 2;

// The cell lies in the floor and the board sits above it on posts. Stacking
// rather than placing them end to end keeps the sled short, and a short sled is
// easier to get into a coupler squarely.
cell_z = floor_thickness;
board_z = cell_z + cell_thickness + 2 * cell_squeeze + comp_height_bottom;

sled_len = board_length + 2 * sled_end_stop;
// Everything has to fit inside the tube bore: the chord at the board height
// must still be wider than the board.
sled_h = board_z + board_thickness + comp_height_top;

assert(sled_h < bore_r * 2 - 1,
       "The stack is taller than the tube bore. Raise tube_id or shrink the stack.");
assert(board_width / 2 < sqrt(max(0.01, pow(bore_r, 2) - pow(sled_h - bore_r, 2))),
       "The board is wider than the tube at the height it sits at. Check board_width.");

// --- the body ---------------------------------------------------------------

// A D-section: the tube bore, flattened off at the top. The curved underside is
// what locates the sled in the tube and stops it rotating.
module sled_blank() {
    intersection() {
        translate([0, 0, bore_r])
            rotate([0, 90, 0])
                cylinder(r = bore_r, h = sled_len);
        translate([0, -bore_r, 0]) cube([sled_len, bore_r * 2, sled_h]);
    }
}

module cell_pocket() {
    translate([(sled_len - cell_length) / 2, 0, cell_z])
        cell_envelope(fit_clearance);
}

// The board sits on four posts with clearance holes through them, so an M2
// screw passes through the board and into the post below. Printed posts take a
// self-tapping M2 without an insert.
module board_posts() {
    translate([sled_end_stop, 0, 0])
        mount_points(d = mount_hole_dia + 2.4, h = board_z);
}

module board_screw_holes() {
    translate([sled_end_stop, 0, -1])
        // Undersized so an M2 cuts its own thread in the plastic.
        mount_points(d = mount_hole_dia - 0.5, h = board_z + 2);
}

// Stops at both ends so the board cannot slide out if a screw backs off, and a
// window through the middle of the floor to save mass and let a strap through.
module end_stops() {
    for (x = [0, sled_len - sled_end_stop])
        translate([x, -bore_r, 0])
            cube([sled_end_stop, bore_r * 2, board_z + board_thickness + sled_end_stop]);
}

module lightening_window() {
    w = board_width * 0.45;
    translate([sled_len * 0.5 - w / 2, -w / 2, -1])
        cube([w, w, floor_thickness + 2]);
}

difference() {
    union() {
        difference() {
            sled_blank();
            cell_pocket();
            lightening_window();
        }
        intersection() {
            board_posts();
            sled_blank();
        }
        intersection() {
            end_stops();
            sled_blank();
        }
    }
    board_screw_holes();
}
