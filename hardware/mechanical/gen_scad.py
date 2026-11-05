import layout as L
pot_body_local = (L.Z_PCB + L.PCB_T + L.POT_BODY_H) - L.Z_PLATE_BOT   # pot can top, plate-local z
strip_top = pot_body_local + 2.0
ox,oy=L.OLED_C
top_holes=[list(h) for i,h in enumerate(L.HOLES) if i not in L.SUPPORT_ONLY]   # screwed from the top
plate=f"""// XO-Hugbox - top plate (generated from layout.py; edit there, or tweak below)
// Units mm. Plate-local z: 0 = underside, {L.PLATE_T} = top face.
W={L.W}; H={L.H}; T={L.PLATE_T}; r=3;
pitch={L.P}; cut=14; clip=1.5; pocket=16;
pot_y={L.POT_Y}; pot_hole=7.5; pot_body_d=17.6; pot_top={pot_body_local:.2f}; strip_top={strip_top:.2f};
oled_c=[{ox:.2f},{oy:.2f}]; oled_pcb=[36.4,34.4]; oled_glass=[35.5,21]; oled_win=[30.5,15.5];
oled_win_off={L.OLED_WIN_OFF}; oled_glass_off={L.OLED_GLASS_OFF}; oled_pcb_d=1.2; oled_glass_d=2.8;
spk=[{L.SPK['x0']},{L.SPK['y0']},{L.SPK['w']},{L.SPK['h']}]; spk_c=[{L.SPK_C[0]:.2f},{L.SPK_C[1]:.2f}]; grille_r=10;
shift=[{L.SHIFT[0]},{L.SHIFT[1]}]; pwr=[{L.PWR[0]},{L.PWR[1]}];
holes={[list(h) for h in L.HOLES]}; top_holes={top_holes};
function cx(i)=8+pitch/2+i*pitch; function cy(j)=8+pitch/2+j*pitch;
module sw(x,y){{ translate([x-cut/2,y-cut/2,-1]) cube([cut,cut,T+2]);
  translate([x-pocket/2,y-pocket/2,-1]) cube([pocket,pocket,T-clip+1]); }}
module box(c,s,z0,z1){{ translate([c[0]-s[0]/2,c[1]-s[1]/2,z0]) cube([s[0],s[1],z1-z0]); }}
difference(){{
  union(){{
    linear_extrude(T) offset(r=r) offset(delta=-r) square([W,H]);
    // raised strip over the pots so the nut clamps 2 mm of plastic above the can
    translate([cx(0)-10.4,pot_y-10.4,0]) linear_extrude(strip_top) offset(r=2) offset(delta=-2)
      square([W-3-(cx(0)-10.4),20.8]);
  }}
  for(i=[0:3]) for(j=[0:3]) sw(cx(i),cy(j));
  translate([7,7,-1]) cube([4*pitch+2,4*pitch+2,T-clip+1]);          // diode clearance
  sw(shift[0],shift[1]);
  // pots: hole, can pocket, base-tab clearance
  for(i=[0:3]) translate([cx(i),pot_y,0]){{
    translate([0,0,-1]) cylinder(d=pot_hole,h=strip_top+2,$fn=48);
    translate([0,0,-1]) cylinder(d=pot_body_d,h=pot_top+1,$fn=72);
  }}
  // 1 mm underside relief over the whole top section (module solder joints, pot tabs), solid around screws
  difference(){{ translate([3,8+4*pitch,-1]) cube([W-6,H-3-(8+4*pitch),2]);
    for(p=holes) translate([p[0],p[1],-2]) cylinder(r=4.5,h=5,$fn=32); }}
  // OLED
  box(oled_c,oled_pcb,-1,oled_pcb_d);
  box([oled_c[0],oled_c[1]+oled_glass_off],oled_glass,-1,oled_glass_d);
  box([oled_c[0],oled_c[1]+oled_win_off],oled_win,-1,T+1);
  // speaker locator + grille
  translate([spk[0],spk[1],-1]) cube([spk[2],spk[3],1.8]);
  for(dx=[-grille_r:4:grille_r]) for(dy=[-grille_r:4:grille_r]) if(dx*dx+dy*dy<=grille_r*grille_r)
    translate([spk_c[0]+dx,spk_c[1]+dy,-1]) cylinder(d=2,h=T+2,$fn=16);
  // power slide switch: body pocket + actuator slot
  box(pwr,[9.4,4.4],-1,3.0); box(pwr,[4.3,2.1],-1,T+1);
  // countersunk M3 from the top (corners only; the pot-strip support post has no screw)
  for(p=top_holes){{ top = T;
    translate([p[0],p[1],0]){{ translate([0,0,-1]) cylinder(d=3.4,h=top+2,$fn=24);
    translate([0,0,top-1.75]) cylinder(d1=3.4,d2=6.6,h=1.76,$fn=32); }} }}
}}
"""
open("xo_hugbox_top_plate.scad","w").write(plate)

usb_z = L.Z_PCB - 2.5 - 1.2 - 1.6
jack_z = L.Z_PCB - 2.5 - 1.6 - 3.0
spk_bottom = L.Z_PLATE_BOT - 15.3
shell=f"""// XO-Hugbox - bottom shell (generated from layout.py)
// z = 0 is the underside. Plate rests on the wall top (z={L.Z_PLATE_BOT}); PCB rests on the bosses (z={L.Z_PCB}).
W={L.W}; H={L.H}; r=3; wall={L.WALL}; floor_t={L.FLOOR}; top={L.Z_PLATE_BOT}; boss_top={L.Z_PCB};
holes={[list(h) for h in L.HOLES]}; top_holes={top_holes};
usb=[{L.XIAO_C[1]},{usb_z:.2f}];     // left wall: y, z centre
jack=[{L.JACK_Y:.2f},{jack_z:.2f}];       // right wall: y, z centre
spk_c=[{L.SPK_C[0]:.2f},{L.SPK_C[1]:.2f}]; ear_pitch={L.SPK_EAR_PITCH};
ear_post_top={(L.Z_PLATE_BOT+0.8)-L.SPK_EAR_DROP-L.SPK_EAR_T/2:.2f};  // underside of the ears (face in the 0.8 mm plate locator)
batt=[46,40]; batt_in=[26,41];        // LiPo 25x40x10 under the key grid
module rr(w,h,rad) offset(r=rad) offset(delta=-rad) square([w,h]);
difference(){{
  union(){{
    difference(){{
      linear_extrude(top) rr(W,H,r);
      translate([wall,wall,floor_t]) linear_extrude(top) rr(W-2*wall,H-2*wall,max(r-wall,0.5));
    }}
    for(p=holes) translate([p[0],p[1],0]) cylinder(d=7,h=boss_top,$fn=40);           // PCB bosses (corner ones take screws, the pot-strip one is a plain support)
    // battery cradle
    translate([batt[0]-batt_in[0]/2-1.2,batt[1]-batt_in[1]/2-1.2,0]) difference(){{
      cube([batt_in[0]+2.4,batt_in[1]+2.4,floor_t+3]);
      translate([1.2,1.2,floor_t]) cube([batt_in[0],batt_in[1],10]);
      translate([1.2+batt_in[0]/2-5,-1,floor_t]) cube([10,batt_in[1]+5,10]);           // finger/lead gaps
    }}
    // posts for the speaker's two screw ears
    for(s=[-1,1]) translate([spk_c[0],spk_c[1]+s*ear_pitch/2,0]) cylinder(d=6.5,h=ear_post_top,$fn=36);
  }}
  for(p=top_holes) translate([p[0],p[1],floor_t]) cylinder(d=2.6,h=boss_top,$fn=24); // M3 self-tap pilot
  for(s=[-1,1]) translate([spk_c[0],spk_c[1]+s*ear_pitch/2,floor_t]) cylinder(d=2.1,h=30,$fn=20); // M2.5 self-tap
  // USB-C (XIAO) through the left wall
  translate([-1,usb[0],usb[1]]) rotate([0,90,0]) linear_extrude(wall+2) offset(r=2) square([7.5-4,12.5-4],center=true);
  // headphone / line-out jack through the right wall
  translate([W-wall-1,jack[0],jack[1]]) rotate([0,90,0]) cylinder(d=8,h=wall+2,$fn=40);
  // rubber-foot recesses
  for(p=[[10,10],[W-10,10],[10,H-10],[W-10,H-10]]) translate([p[0],p[1],-0.01]) cylinder(d=8.5,h=0.8,$fn=40);
}}
"""
open("xo_hugbox_shell.scad","w").write(shell)
print("usb_z",round(usb_z,2),"jack_z",round(jack_z,2),"spk_bottom",round(spk_bottom,2),"strip_top",round(strip_top,2))
