// XO-Hugbox - visual assembly (generated). explode=1 for exploded view.
explode=0; show_shell=1; show_plate=1; show_caps=1; show_bottom=1; e1=explode*55; e2=explode*110; e3=explode*150;
$fn=40;
module keycap(c){ color(c) hull(){ translate([-9,-9,0]) cube([18,18,0.1]); translate([-6.35,-6.35,7.4]) cube([12.7,12.7,0.1]); } }
module mx(x,y,cap){ translate([x,y,0]){
  color("#2b2b2b") translate([-7,-7,18.1]) cube([14,14,5]);
  color("#333") hull(){ translate([-7.8,-7.8,23.1]) cube([15.6,15.6,0.1]); translate([-5.5,-5.5,28.700000000000003]) cube([11,11,0.1]); }
  if(show_caps) translate([0,0,29.700000000000003+e3]) keycap(cap); } }
if(show_shell) color("#d9d4c7") import("xo_hugbox_shell.stl");
if(show_bottom) color("#b8bcc2") translate([46-12.5,40-20,2.2]) cube([25,40,10]);
if(show_bottom) translate([18.3,142.8,0]){
  color("#1b1b1b") translate([-15.5,-14.1,4.600000000000001]) cube([31,28.2,15.3]);
  color("#1b1b1b") for(s=[-1,1]) hull(){ translate([-5.5,s*14.1-0.01,11.500000000000002]) cube([11,0.02,1.5]);
       translate([0,s*18.5,11.500000000000002]) cylinder(d=6.5,h=1.5); }
}
translate([0,0,e1]){
  color("#1f6b3a") translate([0,0,16.5]) linear_extrude(1.6) polygon([[2.4, 2.4], [89.8, 2.4], [89.8, 165.1], [34.6, 165.1], [34.6, 128.1], [2.4, 128.1]]);
  // XIAO (USB-C to the left wall)
  color("#222") translate([2.0,82.0,12.8]) cube([21,17.8,1.2]);
  color("#c0c0c0") translate([3.3000000000000007,83.9,10.0]) cube([12,14,2.8]);
  color("#d8d8d8") translate([1.5,86.4,9.600000000000001]) cube([7.5,9,3.2]);
  color("#1e5bd6") translate([24.8,81.3,12.4]) cube([19.4,18.6,1.6]);
  color("#1e5bd6") translate([47.0,85.5,12.4]) cube([40,15,1.6]);
  color("#7a2a9a") translate([70.5,131.8,12.4]) cube([17.5,33.0,1.6]);
  color("#111") translate([81.0,132.3,6.4]) cube([7,11,6]);
  color("#111") translate([87.0,136.5,9.4]) rotate([0,90,0]) cylinder(d=6,h=3.2);
  for(k=[[17.525, 17.525], [17.525, 36.575], [17.525, 55.625], [17.525, 74.67500000000001], [36.575, 17.525], [36.575, 36.575], [36.575, 55.625], [36.575, 74.67500000000001], [55.625, 17.525], [55.625, 36.575], [55.625, 55.625], [55.625, 74.67500000000001], [74.67500000000001, 17.525], [74.67500000000001, 36.575], [74.67500000000001, 55.625], [74.67500000000001, 74.67500000000001]]) color("#e9e2c8") translate([k[0]-3.8,k[1]-9.5,14.6]) rotate([0,90,0]) cylinder(d=1.9,h=7.6,$fn=12);
  color("#1e5bd6") translate([35.30,129.00,18.900000000000002]) cube([35.4,33.5,1.2]);
  color("#050505") translate([35.75,135.91,20.1]) cube([34.5,21.73,1.4]);
  color("#7fd7ff") translate([39.0,138.8,21.71]) cube([28,1.2,0.05]);
  color("#7fd7ff") translate([39.0,148.3,21.71]) cube([18,3,0.05]);
  for(i=[0:5]) color("#7fd7ff") translate([39.0+i*4.8,140.8,21.71]) cube([3,1+i*1.2,0.05]);
  // pots - pin tab faces the keys
  for(i=[0:3]) translate([8.0+19.05/2+i*19.05,117.0,0]){
    color("#c8c8c8") translate([0,0,18.1]) cylinder(d=16.27,h=7.12);
    color("#d0d0d0") translate([0,0,25.220000000000002]) cylinder(d=6.65,h=7.33);
    color("#e0e0e0") translate([0,0,32.550000000000004]) cylinder(d=5.95,h=8.26);
    // straight solder lugs toward the keys, wire legs down into the oval slots
    for(k=[-1:1]){ color("#d8c27a") translate([k*5.0-1,-(12.515),23.380000000000003]) cube([2,4.88,0.5]);
      color("#bbb") translate([k*5.0,-(11.315000000000001),15.3]) cylinder(d=0.6,h=8.08,$fn=8); }
    if(show_caps) translate([0,0,29.1+e3]){ color("#2a2a2a") cylinder(d=14.35,h=16.8,$fn=24); color("#bbbbbb") translate([0,0,16.8]) cylinder(d=12,h=0.3); color("white") translate([-0.5,-6,17.1]) cube([1,4.5,0.2]); }
    if(show_caps) color("#999") translate([0,0,27.1+e2]) difference(){ cylinder(d=11,h=2,$fn=6); cylinder(d=7,h=5,center=true); }
  }
  color("#c8c8c8") translate([75.15,152.65,18.1]) cube([8.7,3.7,3.7]);
  color("#111") translate([78.0,153.75,21.8]) cube([1.5,1.5,5]);
  for(k=[[17.525, 17.525], [17.525, 36.575], [17.525, 55.625], [17.525, 74.67500000000001], [36.575, 17.525], [36.575, 36.575], [36.575, 55.625], [36.575, 74.67500000000001], [55.625, 17.525], [55.625, 36.575], [55.625, 55.625], [55.625, 74.67500000000001], [74.67500000000001, 17.525], [74.67500000000001, 36.575], [74.67500000000001, 55.625], [74.67500000000001, 74.67500000000001]]) mx(k[0],k[1],"#ece9e1");
  mx(80.5,139.5,"#e8792b");
}
if(show_plate) translate([0,0,19.1+e2]) color("#f2efe6") import("xo_hugbox_top_plate.stl");
