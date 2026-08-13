// Form B, upper half: covers the board and carries the static port marks.
//
//   openscad -o oapogee-pod-cap.stl pod-cap.scad
//   openscad -D port_dia=3 -o cap-drilled.stl pod-cap.scad   ports bored, not marked
//
// Print the flat split face down, no supports. The aerodynamic surface then
// faces up and is never a bridge.

include <pod-shape.scad>

difference() {
    union() {
        difference() {
            intersection() {
                pod_outer();
                above_split();
            }
            // Grown by a print clearance so the base spigot slides in.
            cavity(fit_clearance / 2);
        }
        intersection() {
            cap_bosses();
            intersection() {
                pod_outer();
                above_split();
            }
        }
    }
    cap_screw_holes();
    port_marks();
}
