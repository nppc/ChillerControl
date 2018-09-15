$fn=20;

/*
translate([0,-18,5]){
    translate([10,0,0])color("green")CoolerControlPCB();
    translate([0,40,0])color("green")HWBuckPCB();
}
*/



//difference(){
//    union(){
        //bottom();
        top();
//    }
    //translate([-50,-7,0])cube([100,100,100]);
//}
    
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
difference(){union(){
    difference(){
        union(){difference(){
            hull(){
                translate([-72/2+4,-84/2+4,2])cylinder(d=8,h=3,$fn=30);
                translate([72/2-4,-84/2+4,2])cylinder(d=8,h=3,$fn=30);

                translate([-62/2+4,-70/2+4,32])cylinder(d=8,h=3,$fn=30);
                translate([62/2-4,-70/2+4,32])cylinder(d=8,h=3,$fn=30);

                translate([-72/2+3/2,84/2-3/2,2])cylinder(d=3,h=3,$fn=20);
                translate([72/2-3/2,84/2-3/2,2])cylinder(d=3,h=3,$fn=20);

                translate([-62/2+3/2,84/2-3/2,32])cylinder(d=3,h=3,$fn=20);
                translate([62/2-3/2,84/2-3/2,32])cylinder(d=3,h=3,$fn=20);
            }
            translate([0,0,-1])hull(){
                translate([-72/2+4,-84/2+4,2])cylinder(d=6,h=3,$fn=30);
                translate([72/2-4,-84/2+4,2])cylinder(d=6,h=3,$fn=30);

                translate([-62/2+4,-70/2+4,32])cylinder(d=6,h=3,$fn=30);
                translate([62/2-4,-70/2+4,32])cylinder(d=6,h=3,$fn=30);

                translate([-72/2+3/2,84/2-3/2,2])cylinder(d=1,h=3,$fn=20);
                translate([72/2-3/2,84/2-3/2,2])cylinder(d=1,h=3,$fn=20);

                translate([-62/2+3/2,84/2-3/2,32])cylinder(d=1,h=3,$fn=20);
                translate([62/2-3/2,84/2-3/2,32])cylinder(d=1,h=3,$fn=20);
            }
            }
            translate([-38.2,-7,10])rotate([0,90,0])translate([0,0,2.2])hull(){
                cylinder(d1=20, d2=23, h=4,$fn=40);
                translate([9,-24/2,0])cube([1,24,4]);
            }
        }
    // cutout for connector
        translate([-38.2,-7,10])rotate([0,90,0])translate([0,0,2.2]){
            hull(){
                cylinder(d=19.2, h=1,$fn=40);
                translate([9,-23.2/2,0])cube([1,23.2,1]);
            }
            translate([0,0,1.9])hull(){
                cylinder(d=19.2, h=3,$fn=40);
                translate([9,-23/2,0])cube([1,23,3]);
            }
            hull(){
                cylinder(d=18.2, h=4,$fn=40);
                translate([9,-22.2/2,0])cube([1,22.2,4]);
            }
        }

        // cutouts
        for(i=[-25:10:25])
            translate([i,40,19])cube([7,50,28],true);

        for(i=[-25:10:25])
            translate([i,-30,19]){
                rotate([90,0,0])hull(){
                    translate([0,-7,0])cylinder(d=4,h=40,$fn=20);
                    translate([0,10,0])cylinder(d=4,h=40,$fn=20);
                }
            }

        for(i=[-24:6:25]){
            translate([i,-17,30]){
                hull(){
                    translate([0,-8,0])cylinder(d=2,h=40,$fn=20);
                    translate([0,8,0])cylinder(d=2,h=40,$fn=20);
                }
            }
            translate([i,27,30]){
                hull(){
                    translate([0,-8,0])cylinder(d=2,h=40,$fn=20);
                    translate([0,8,0])cylinder(d=2,h=40,$fn=20);
                }
            }
        }
        // for base
        hull(){
            translate([-72/2+4,-84/2+4,0])cylinder(d=6.2,h=4.1,$fn=30);
            translate([72/2-4,-84/2+4,0])cylinder(d=6.2,h=4.1,$fn=30);

            translate([-72/2+1.5,84/2-1.5,0])cylinder(d=1.2,h=4.1,$fn=15);
            translate([72/2-1.5,84/2-1.5,0])cylinder(d=1.2,h=4.1,$fn=15);
        }
        // locks
        translate([-34.8,25,3.5])rotate([90,0,0])cylinder(d=1.2,h=7,center=true);
        translate([-34.8,-25,3.5])rotate([90,0,0])cylinder(d=1.2,h=7,center=true);
        translate([34.8,25,3.5])rotate([90,0,0])cylinder(d=1.2,h=7,center=true);
        translate([34.8,-25,3.5])rotate([90,0,0])cylinder(d=1.2,h=7,center=true);
        translate([0,-40.8,3.5])rotate([90,0,90])cylinder(d=1.2,h=7,center=true);
        translate([0,40.8,3.5])rotate([90,0,90])cylinder(d=1.2,h=7,center=true);

        
    }
translate([20,42,5])rotate([90,0,0])cylinder(d=8,h=1);
for(i=[-20:10:21])translate([i,40.5,19.5])cube([1,2,30],true);
    
}
translate([20,42,5])hull(){
    rotate([90,0,0])cylinder(d=4,h=5, center=true);
    translate([0,0,-5])cube([4,5,1],true);
}
translate([-36,-33,5])hull(){
    rotate([90,0,90])cylinder(d=4,h=5, center=true);
    translate([0,0,-5])cube([5,4,1],true);
}
translate([0,0,-0.9])cube([100,100,6],true);

}
}

module bottom(){
difference(){    
    union(){
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
        // ventilation holes
        for(i=[-27:7:18])
        translate([i,25,19]){
            hull(){
                translate([0,-7,0])cylinder(d=3,h=40, center=true);
                translate([0,7,0])cylinder(d=3,h=40, center=true);
            }
        }
        for(i=[-7:7:30])
        translate([i,-18,19]){
            hull(){
                translate([0,-10,0])cylinder(d=3,h=40, center=true);
                translate([0,10,0])cylinder(d=3,h=40, center=true);
            }
        }
        translate([-36,-33,5])rotate([90,0,90])cylinder(d=4,h=5, center=true);
        translate([20,42,5])rotate([90,0,0])cylinder(d=4,h=5, center=true);

    }

    translate([0,-18,6]){
        translate([10,0,0]){
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
    connectorHolder();
    // locks
    translate([-34.8,25,3.5])rotate([90,0,0])cylinder(d=1,h=6,center=true);
    translate([-34.8,-25,3.5])rotate([90,0,0])cylinder(d=1,h=6,center=true);
    translate([34.8,25,3.5])rotate([90,0,0])cylinder(d=1,h=6,center=true);
    translate([34.8,-25,3.5])rotate([90,0,0])cylinder(d=1,h=6,center=true);
    translate([0,-40.8,3.5])rotate([90,0,90])cylinder(d=1,h=6,center=true);
    translate([0,40.8,3.5])rotate([90,0,90])cylinder(d=1,h=6,center=true);

    
    }
    translate([0,-18,6]){
        translate([10,0,0]){
            translate([41/2,37/2,-7])cylinder(d=1.5,h=10,$fn=15);
            translate([-41/2,37/2,-7])cylinder(d=1.5,h=10,$fn=15);
            translate([-41/2,-37/2,-7])cylinder(d=1.5,h=10,$fn=15);
            translate([41/2,-37/2,-7])cylinder(d=1.5,h=10,$fn=15);
        }
        translate([0,40,0]){
            translate([-31,-32/2+2,-6])cylinder(d=1.5,h=10,$fn=15);
            translate([66/2-9,32/2-6.26,-6])cylinder(d=1.5,h=10,$fn=15);
        }
    }
}
}

module connector(){
    cylinder(d=12.5,h=2.2,$fn=20);
    cylinder(d=11,h=17,$fn=20);
    cylinder(d=2.5,h=22,$fn=20);
}

module connectorHolder(){
    translate([-38.2,-7,10])rotate([0,90,0]){
        difference(){
            translate([0,0,2.2]){
                hull(){
                    cylinder(d=18, h=4,$fn=40);
                    translate([9,-22/2,0])cube([1,22,4]);
                }
                hull(){
                    cylinder(d=19, h=0.8,$fn=40);
                    translate([9,-23/2,0])cube([1,23,0.8]);
                }
                translate([0,0,2])hull(){
                    cylinder(d=19, h=2,$fn=40);
                    translate([9,-23/2,0])cube([1,23,2]);
                }
            }
            connector();
        }
    }
}

