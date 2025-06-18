// Parameters
leg_width = 0.10;           // Width of each leg
leg_depth = 0.90;           // Depth/thickness of each leg
leg_spacing = 1.3;         // Space between the two legs
table_height = 0.85;      // Height of the table top
top_thickness = 0.10;       // Thickness of the tabletop
pin_diameter = 0.10;       // Diameter of the alignment pins
pin_spacing = 0.54;        // Center-to-center spacing between pins
pin_height = 0.12;          // Height of alignment pins

// Build each leg
module leg(x_offset) {
    translate([x_offset, 0, 0])
        cube([leg_width, leg_depth, table_height]);
}

// Build tabletop
module tabletop() {
    translate([0, 0, table_height])
        cube([2 * leg_width + leg_spacing, leg_depth, top_thickness]);
}


// Build alignment pins on top
module pin(x_offset) {
    translate([x_offset, leg_depth * (1/4), table_height + top_thickness])
        cylinder(h = pin_height, d = pin_diameter, $fn = 16);
}


// Assemble standoff
union() {
    leg(0);
    leg(leg_width + leg_spacing);
    tabletop();

    // Pins, centered on tabletop
    pin((2 * leg_width + leg_spacing) / 2 - pin_spacing / 2);
    pin((2 * leg_width + leg_spacing) / 2 + pin_spacing / 2);
}
