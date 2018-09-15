/*
translate([0,-18,5]){
    translate([5,0,0])color("green")CoolerControlPCB();
    translate([0,40,0])color("green")HWBuckPCB();
}
*/
bottom();
top();

module CoolerControlPCB(){
    translate([0,0,0.8])cube([45,41,1.6],true);
    translate([41/2,37/2,-6])cylinder(d=1.5,h=8,$fn=15);
    translate([-41/2,37/2,-6])cylinder(d=1.5,h=8,$fn=15);
    translate([-41/2,-37/2,-6])cylinder(d=1.5,h=8,$fn=15);
    translate([41/2,-37/2,-6])cylinder(d=1.5,h=8,$fn=15);
    translate([0,5,15/2+1.6])cube([26,31,15],true);
}

module HWBuckPCB(){
    translate([0,0,0.8])cube([66,32,1.6],true);
    translate([-31,-32/2+2,-6])cylinder(d=1.5,h=8,$fn=15);
    translate([66/2-9,32/2-6.26,-6])cylinder(d=1.5,h=8,$fn=15);
    cylinder(d=15,h=26+1.6,$fn=15);
}

module top(){
    hull(){
        translate([-72/2+4,-84/2+4,2])cylinder(d=8,h=3,$fn=30);
        translate([72/2-4,-84/2+4,2])cylinder(d=8,h=3,$fn=30);

        translate([-68/2+4,-76/2+4,22])cylinder(d=8,h=3,$fn=30);
        translate([68/2-4,-76/2+4,22])cylinder(d=8,h=3,$fn=30);

        translate([0,30,3/2+2])cube([72,1,3],true);
        translate([0,30,3/2+22])cube([68,1,3],true);
    }

    hull(){
        translate([-72/2+3/2,84/2-3/2,2])cylinder(d=3,h=3,$fn=20);
        translate([72/2-3/2,84/2-3/2,2])cylinder(d=3,h=3,$fn=20);

        translate([-65.8/2+3/2,84/2-3/2,33])cylinder(d=3,h=3,$fn=20);
        translate([65.8/2-3/2,84/2-3/2,33])cylinder(d=3,h=3,$fn=20);

        translate([-72/2+8/2,5,2])cylinder(d=8,h=3,$fn=20);
        translate([72/2-8/2,5,2])cylinder(d=8,h=3,$fn=20);

        translate([-65.8/2+8/2,20,33])cylinder(d=8,h=3,$fn=20);
        translate([65.8/2-8/2,20,33])cylinder(d=8,h=3,$fn=20);
    }
}

module bottom(){
    difference(){
    union(){
        hull(){
            translate([-72/2+4,-84/2+4,0])cylinder(d=8,h=2,$fn=30);
            translate([72/2-4,-84/2+4,0])cylinder(d=8,h=2,$fn=30);

            translate([-72/2+1.5,84/2-1.5,0])cylinder(d=3,h=2,$fn=15);
            translate([72/2-1.5,84/2-1.5,0])cylinder(d=3,h=2,$fn=15);
        }

        hull(){
            translate([-72/2+4,-84/2+4,0])cylinder(d=6,h=4,$fn=30);
            translate([72/2-4,-84/2+4,0])cylinder(d=6,h=4,$fn=30);

            translate([-72/2+1.5,84/2-1.5,0])cylinder(d=1,h=4,$fn=15);
            translate([72/2-1.5,84/2-1.5,0])cylinder(d=1,h=4,$fn=15);
        }
    }
        hull(){
            translate([0,10,4/2+1])cube([68,60,4], true);
            translate([-72/2+4,-84/2+4,1])cylinder(d=4,h=4,$fn=20);
            translate([72/2-4,-84/2+4,1])cylinder(d=4,h=4,$fn=20);
        }
    }

    translate([0,-18,6]){
        translate([5,0,0]){
        translate([41/2,37/2,-6])cylinder(d1=7,d2=4,h=5,$fn=15);
        translate([-41/2,37/2,-6])cylinder(d1=7,d2=4,h=5,$fn=15);
        translate([-41/2,-37/2,-6])cylinder(d1=7,d2=4,h=5,$fn=15);
        translate([41/2,-37/2,-6])cylinder(d1=7,d2=4,h=5,$fn=15);
    }
     translate([0,40,0]){
        translate([-31,-32/2+2,-6])cylinder(d1=7,d2=4,h=5,$fn=15);
        translate([66/2-9,32/2-6.26,-6])cylinder(d1=7,d2=4,h=5,$fn=15);
     }
    }
}