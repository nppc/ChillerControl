$fn=80;
difference(){
    cover();
    cooler();
}
translate([0,0,4])rest();

module rest(){
    for(i=[10:10:60]){
        difference(){
            cylinder(d=i+3,h=2);
            cylinder(d=i,h=2);
        }
    }
    cylinder(d=10,h=2);

    for(i=[0:45:150]){
        rotate([0,0,i])translate([0,0,1])cube([2,72,2],true);
    }

}

module cover(){
    cylinder(d1=70,d2=74,h=6);
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
    cylinder(d1=68,d2=71,h=6);
    translate([0,0,6])cylinder(d=71,h=10);
    translate([62/2,62/2,0])cylinder(d=4.5,h=30,$fn=20, center=true);
    translate([-62/2,62/2,0])cylinder(d=4.5,h=30,$fn=20, center=true);
    translate([62/2,-62/2,0])cylinder(d=4.5,h=30,$fn=20, center=true);
    translate([-62/2,-62/2,0])cylinder(d=4.5,h=30,$fn=20, center=true);
}