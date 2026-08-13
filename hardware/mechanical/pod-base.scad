// Form B, lower half: carries the cell, takes the straps, sits on the tube.
//
//   openscad -o oapogee-pod-base.stl pod-base.scad
//   openscad -D tube_od=41.6 -o base-bt60.stl pod-base.scad   a different airframe
//
// Print the flat split face down, no supports. That puts the saddle facing
// upward, where a concave surface costs nothing, and gives the first layer the
// largest flat face on the part.

include <pod-shape.scad>

difference() {
    union() {
        difference() {
            intersection() {
                pod_outer();
                below_split();
            }
            saddle();
            cavity();
            strap_slots();
        }
        bosses();
        lip();
    }
    boss_pilots();
}
