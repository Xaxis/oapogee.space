// Shapes both printed parts share.
//
// Everything here is driven by params.scad, which is generated from
// data/mechanical.yaml. No dimension is written in this file, on purpose: a
// number typed into geometry is a number the site cannot see and a reviewer
// cannot check the provenance of.

include <params.scad>

$fn = 64;

// The board and the cell, as solids to subtract. Rendered on their own by
// fit.scad so you can see what the cavity is holding.
module board_envelope(clearance = 0) {
    c = clearance;
    translate([-c, -board_width / 2 - c, -c])
        cube([board_length + 2 * c, board_width + 2 * c, board_thickness + 2 * c]);
}

module board_components(clearance = 0) {
    c = clearance;
    // Component keepout above and below, inset from the edge so the envelope
    // stays the thing that sets the fit.
    translate([-c, -board_width / 2 - c, board_thickness])
        cube([board_length + 2 * c, board_width + 2 * c, comp_height_top + c]);
    translate([-c, -board_width / 2 - c, -comp_height_bottom - c])
        cube([board_length + 2 * c, board_width + 2 * c, comp_height_bottom + c]);
}

module cell_envelope(clearance = 0) {
    c = clearance + cell_squeeze;
    translate([-c, -(cell_width + 2 * c) / 2, -c])
        cube([cell_length + 2 * c, cell_width + 2 * c, cell_thickness + 2 * c]);
}

// The four board mounting holes, as posts or as holes depending on what is
// wrapped around them.
module mount_points(d, h) {
    for (x = [mount_hole_inset, board_length - mount_hole_inset])
        for (y = [-board_width / 2 + mount_hole_inset, board_width / 2 - mount_hole_inset])
            translate([x, y, 0]) cylinder(d = d, h = h);
}

// A rounded slab used as a station in a hull. Hulling several of these along X
// gives a smooth streamlined body without a surface of revolution, which is
// what a pod wants: it has to be wide and flat, not round.
module station(x, w, h, r) {
    translate([x, 0, 0])
        hull()
            for (sy = [-1, 1])
                for (sz = [0, 1])
                    translate([0, sy * (w / 2 - r), sz * (h - 2 * r) + r])
                        rotate([0, 90, 0])
                            cylinder(r = r, h = 0.01, center = true);
}

// The static port pilot marks.
//
// Shallow cones, not holes. The Mounting page will not publish a port diameter
// until it has been derived from the enclosed volume and confirmed by flying
// two configurations, and a model that shipped a default would make that
// refusal false in the one place a builder acts on it. So the pod tells you
// where to drill, which is known, and not how big, which is not.
//
// Set port_dia on the command line to bore them for real:
//   openscad -D port_dia=3 -o pod.stl pod.scad
module port_mark(depth) {
    if (port_dia == undef) {
        // Cone rather than cylinder: a drill bit centres itself in a cone, and
        // a mark you cannot start a bit in is decoration.
        rotate([0, 90, 0]) cylinder(r1 = depth, r2 = 0, h = depth);
    } else {
        assert(port_dia > 0, "port_dia must be positive");
        rotate([0, 90, 0]) cylinder(d = port_dia, h = depth * 4, center = true);
    }
}
