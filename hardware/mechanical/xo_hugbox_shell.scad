// XO-Hugbox - bottom shell (generated from layout.py)
// z = 0 is the underside. Plate rests on the wall top (z=19.1); PCB rests on the bosses (z=16.5).
W=92.2; H=167.5; r=3; wall=2.0; floor_t=2.0; top=19.1; boss_top=16.5;
holes=[[6, 6], [86.2, 6], [6, 70], [86.2, 70], [86.2, 112.0]]; top_holes=[[6, 6], [86.2, 6], [6, 70], [86.2, 70]];
usb=[90.9,11.20];     // left wall: y, z centre
jack=[136.50,9.40];       // right wall: y, z centre
spk_c=[18.30,142.80]; ear_pitch=37.0;
ear_post_top=11.50;  // underside of the ears (face in the 0.8 mm plate locator)
batt=[46,40]; batt_in=[26,41];        // LiPo 25x40x10 under the key grid
module rr(w,h,rad) offset(r=rad) offset(delta=-rad) square([w,h]);
difference(){
  union(){
    difference(){
      linear_extrude(top) rr(W,H,r);
      translate([wall,wall,floor_t]) linear_extrude(top) rr(W-2*wall,H-2*wall,max(r-wall,0.5));
    }
    for(p=holes) translate([p[0],p[1],0]) cylinder(d=7,h=boss_top,$fn=40);           // PCB bosses (corner ones take screws, the pot-strip one is a plain support)
    // battery cradle
    translate([batt[0]-batt_in[0]/2-1.2,batt[1]-batt_in[1]/2-1.2,0]) difference(){
      cube([batt_in[0]+2.4,batt_in[1]+2.4,floor_t+3]);
      translate([1.2,1.2,floor_t]) cube([batt_in[0],batt_in[1],10]);
      translate([1.2+batt_in[0]/2-5,-1,floor_t]) cube([10,batt_in[1]+5,10]);           // finger/lead gaps
    }
    // posts for the speaker's two screw ears
    for(s=[-1,1]) translate([spk_c[0],spk_c[1]+s*ear_pitch/2,0]) cylinder(d=6.5,h=ear_post_top,$fn=36);
  }
  for(p=top_holes) translate([p[0],p[1],floor_t]) cylinder(d=2.6,h=boss_top,$fn=24); // M3 self-tap pilot
  for(s=[-1,1]) translate([spk_c[0],spk_c[1]+s*ear_pitch/2,floor_t]) cylinder(d=2.1,h=30,$fn=20); // M2.5 self-tap
  // USB-C (XIAO) through the left wall
  translate([-1,usb[0],usb[1]]) rotate([0,90,0]) linear_extrude(wall+2) offset(r=2) square([7.5-4,12.5-4],center=true);
  // headphone / line-out jack through the right wall
  translate([W-wall-1,jack[0],jack[1]]) rotate([0,90,0]) cylinder(d=8,h=wall+2,$fn=40);
  // rubber-foot recesses
  for(p=[[10,10],[W-10,10],[10,H-10],[W-10,H-10]]) translate([p[0],p[1],-0.01]) cylinder(d=8.5,h=0.8,$fn=40);
}
