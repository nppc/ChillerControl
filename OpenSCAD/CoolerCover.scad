$fn=100;
difference(){
    cover();
    cooler();
}
translate([0,0,4])rest();

module rest(){
    for(i=[10:11:68]){
        difference(){
            cylinder(d=i+2.2,h=2);
            cylinder(d=i,h=2);
        }
    }
    cylinder(d=10,h=2);

    translate([0,0,1])cube([2,70,2],true);
    rotate([0,0,90])translate([0,0,1])cube([2,70,2],true);
    rotate([0,0,45])translate([0,0,1])cube([2,77,2],true);
    rotate([0,0,45+90])translate([0,0,1])cube([2,77,2],true);
}

module cover(){
    difference(){
        cylinder(d1=77,d2=79,h=6);
        difference(){
            cube([100,100,50],true);
            cube([70,70,52],true);
        }
    }

    for(i=[0:90:270]){
        rotate([0,0,i])difference(){
            hull(){
                translate([62/2,62/2,0])cylinder(d=8,h=1.5,$fn=$fn/2);
                translate([0,0,5])rotate([0,0,45])cube([30,30,10],true);
            }
            translate([62/2,62/2,1.5])cylinder(d=8,h=5,$fn=$fn/2);
        }
    }
}

module cooler(){
    translate([0,0,-5])cube([70,70,10],true);
    difference(){
        cylinder(d1=75,d2=77,h=6);
        difference(){
            cube([100,100,50],true);
            cube([68,68,52],true);
        }
    }
    translate([0,0,6])cylinder(d=68,h=10);
    translate([62/2,62/2,0])cylinder(d=4.5,h=30,$fn=20, center=true);
    translate([-62/2,62/2,0])cylinder(d=4.5,h=30,$fn=20, center=true);
    translate([62/2,-62/2,0])cylinder(d=4.5,h=30,$fn=20, center=true);
    translate([-62/2,-62/2,0])cylinder(d=4.5,h=30,$fn=20, center=true);
}