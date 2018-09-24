lockWidth=20;
lockDiam=6;
coverThick=42;

PRINT=1;
PRINTONLYJOINT=0;


if(PRINT==1){
    if(PRINTONLYJOINT!=1){
        translate([0,0,1.5])lockSide();
        translate([0,35,0])rotate([90,0,0])lockUp();
    }
    translate([70,-10,-1])traat();
}else{
    lockSide();
    translate([0,coverThick+22,0])lockUp();
    traat();
}

module lockSide(){
    difference(){
        hull(){
            translate([-lockWidth/2,0,0])cube([lockWidth,10,8]);
            translate([-lockWidth/2,20,0])cube([lockWidth,2,2]);
            translate([0,20,6])rotate([0,90,0])cylinder(d=4,h=lockWidth, center=true, $fn=20);
        }
        translate([0,-2,4])rotate([0,90,0])cylinder(d=lockDiam+0.5,h=lockWidth, center=true, $fn=20);
    }
    //translate([-(lockWidth+50)/2,-18,-1.5])cube([lockWidth+50,40,2]);
    hull(){
        translate([-(lockWidth+50)/2+3,-18+3,-1.5])cylinder(d=6,h=2,$fn=30);
        translate([(lockWidth+50)/2-3,-18+3,-1.5])cylinder(d=6,h=2,$fn=30);
        translate([-(lockWidth+50)/2+3,18+1,-1.5])cylinder(d=6,h=2,$fn=30);
        translate([(lockWidth+50)/2-3,18+1,-1.5])cylinder(d=6,h=2,$fn=30);
    }
}

module lockUp(){
    difference(){
        hull(){
            translate([-lockWidth/2,8,0])cube([lockWidth,5,8]);
        translate([0,2,-33])rotate([90,0,0])cylinder(d=20,h=2,$fn=50);
            translate([0,2,6])rotate([0,90,0])cylinder(d=4,h=lockWidth, center=true, $fn=20);
        }
        translate([0,14,4])rotate([0,90,0])cylinder(d=lockDiam+0.5,h=lockWidth, center=true, $fn=20);
    }
    hull(){
        translate([-(lockWidth+50)/2-2,2,-3])rotate([90,0,0])cylinder(d=6,h=2,$fn=30);
        translate([(lockWidth+50)/2+2,2,-3])rotate([90,0,0])cylinder(d=6,h=2,$fn=30);
        translate([-(lockWidth+50)/2-2,2,-10])rotate([90,0,0])cylinder(d=6,h=2,$fn=30);
        translate([(lockWidth+50)/2+2,2,-10])rotate([90,0,0])cylinder(d=6,h=2,$fn=30);
        translate([0,2,-33])rotate([90,0,0])cylinder(d=23,h=2,$fn=50);
    }
}

module traat(){
    translate([0,-2,4])rotate([0,90,0])cylinder(d=lockDiam,h=lockWidth+2, center=true, $fn=20);

    translate([0,coverThick+36,4])rotate([0,90,0])cylinder(d=lockDiam,h=lockWidth+2, center=true, $fn=20);
    
    translate([15,0,0])hull(){
        translate([0,-2,4])rotate([0,90,0])cylinder(d=lockDiam,h=8, center=true, $fn=20);
        translate([0,coverThick+36,4])rotate([0,90,0])cylinder(d=lockDiam,h=8, center=true, $fn=20);
        translate([0,coverThick+36-1,2])cube([8,8,2],true);
        translate([0,-1,2])cube([8,8,2],true);
    }

    translate([-15,0,0])hull(){
        translate([0,-2,4])rotate([0,90,0])cylinder(d=lockDiam,h=8, center=true, $fn=20);
        translate([0,coverThick+36,4])rotate([0,90,0])cylinder(d=lockDiam,h=8, center=true, $fn=20);
        translate([0,coverThick+36-1,2])cube([8,8,2],true);
        translate([0,-1,2])cube([8,8,2],true);
    }

}