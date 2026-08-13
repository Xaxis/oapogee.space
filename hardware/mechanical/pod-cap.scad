// Form B, upper half: covers the board and carries the static port marks.
//
//   openscad -o oapogee-pod-cap.stl pod-cap.scad
//   openscad -D port_dia=3 -o cap-drilled.stl pod-cap.scad   ports bored, not marked
//
// Print the flat split face down, no supports. The aerodynamic surface then
// faces up and is never a bridge.

include <pod-shape.scad>

difference() {
    intersection() {
        pod_outer();
        above_split();
    }
    cavity();
    cap_screw_holes();
    port_marks();
}
