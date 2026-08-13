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
// Print deck down, no supports. Print one and check it slides in your own tube
// before printing anything else: paper tube bore varies enough between
// production runs that this is worth five minutes.
//
// The sled has no static ports. It does not enclose the sensor, the airframe
// does, so the ports go in the body tube and the Mounting page covers where.
//
// WHY THE BOARD AND THE CELL LIE END TO END
//
// The first version stacked them, cell in the floor and board on posts above.
// That cannot work in a tube this size and the geometry says so plainly: a
// 20 mm wide cell only fits inside a 24.1 mm bore between z = 7.1 and z = 16.7,
// a 20 mm board has the same constraint, and the two intervals are the same
// interval. Stacking spends the one dimension a payload bay has plenty of.
//
// So the sled is a deck across the widest part of the bore, with the board at
// one end and the cell at the other. It is longer and it fits.

include <common.scad>

// --- derived geometry -------------------------------------------------------

bore_r = (tube_id - fit_clearance) / 2;

// The deck sits at the bore centreline, the only height where the tube is at
// full width. Anywhere lower and the chord is narrower than the board.
deck_top = bore_r;

board_zone = board_length + 2 * fit_clearance;
cell_zone = cell_length + 2 * (fit_clearance + cell_squeeze);
cell_wide = cell_width + 2 * (fit_clearance + cell_squeeze);

board_x = sled_end_stop;
divider_x = board_x + board_zone;
cell_x = divider_x + sled_end_stop;
sled_len = cell_x + cell_zone + sled_end_stop;

// Everything rides on the deck, so the only width that matters is the chord at
// deck_top, which is the full bore.
chord = 2 * bore_r;
assert(board_width + 2 * fit_clearance < chord,
       "The board is wider than the tube bore. Reduce board_width or raise tube_id.");
assert(cell_wide < chord,
       "The cell is wider than the tube bore. Reduce cell_width or raise tube_id.");

// --- the body ---------------------------------------------------------------

module bore() {
    translate([0, 0, bore_r]) rotate([0, 90, 0]) cylinder(r = bore_r, h = sled_len);
}

// A plate across the middle of the bore, exactly floor_thickness thick.
// Intersecting with the tube gives the curved edges that locate the sled and
// stop it rotating, for free.
//
// Not a slab of floor_thickness + sled_rail_height: that is solid all the way
// across, so the blind screw holes ended up roofed by 0.9 mm of plastic and the
// board could never be fastened to it. The rails are edges, not a filled block.
module sled_blank() {
    intersection() {
        bore();
        translate([0, -bore_r, deck_top - floor_thickness])
            cube([sled_len, bore_r * 2, floor_thickness]);
    }
}

// Thin walls along both long edges, rising above the deck. They keep the sled
// from rocking and give the cell something to sit between.
module rails() {
    rail_t = pod_wall;
    difference() {
        intersection() {
            bore();
            translate([0, -bore_r, deck_top - floor_thickness])
                cube([sled_len, bore_r * 2, floor_thickness + sled_rail_height]);
        }
        translate([-1, -(chord / 2 - rail_t), deck_top - floor_thickness - 1])
            cube([sled_len + 2, chord - 2 * rail_t, floor_thickness + sled_rail_height + 2]);
    }
}

// The board stands off the deck far enough for whatever is on its underside.
//
// Everything that rises from the deck starts inside it rather than on it. A
// cylinder whose base sits exactly on the deck face touches with zero overlap,
// and CGAL exports that as a separate solid: the first version of this sled
// came out as a deck and four loose posts.
module board_posts() {
    translate([board_x + fit_clearance, 0, deck_top - floor_thickness])
        mount_points(d = mount_hole_dia + 2.4, h = floor_thickness + comp_height_bottom);
}

module board_screw_holes() {
    // Undersized so an M2 cuts its own thread. Blind: it stops inside the deck
    // rather than breaking through the underside.
    translate([board_x + fit_clearance, 0, deck_top - floor_thickness / 2])
        mount_points(d = mount_hole_dia - 0.5, h = comp_height_bottom + floor_thickness);
}

// Walls at both ends and between the two zones, so nothing slides along the
// tube and the cell cannot reach the board.
module walls() {
    for (x = [0, divider_x, sled_len - sled_end_stop])
        translate([x, -bore_r, deck_top - floor_thickness])
            cube([sled_end_stop, bore_r * 2, floor_thickness + cell_thickness + cell_squeeze]);
}

// Under the board only. The cell end stays solid: it is what the cell lies on.
module lightening_window() {
    w = board_width * 0.45;
    l = board_zone * 0.5;
    translate([board_x + board_zone / 2 - l / 2, -w / 2, deck_top - floor_thickness - 1])
        cube([l, w, floor_thickness + 2]);
}

difference() {
    union() {
        difference() {
            sled_blank();
            lightening_window();
        }
        intersection() {
            union() {
                board_posts();
                walls();
                rails();
            }
            translate([0, 0, bore_r]) rotate([0, 90, 0]) cylinder(r = bore_r, h = sled_len);
        }
    }
    board_screw_holes();
}
