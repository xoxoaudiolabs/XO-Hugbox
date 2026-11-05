// XO-Hugbox - top plate (generated from layout.py; edit there, or tweak below)
// Units mm. Plate-local z: 0 = underside, 4.0 = top face.
W=92.2; H=167.5; T=4.0; r=3;
pitch=19.05; cut=14; clip=1.5; pocket=16;
pot_y=117.0; pot_hole=7.5; pot_body_d=17.6; pot_top=6.00; strip_top=8.00;
oled_c=[53.00,145.70]; oled_pcb=[36.4,34.4]; oled_glass=[35.5,21]; oled_win=[30.5,15.5];
oled_win_off=0.7; oled_glass_off=3.4; oled_pcb_d=1.2; oled_glass_d=2.8;
spk=[2.6,128.5,31.4,28.6]; spk_c=[18.30,142.80]; grille_r=10;
shift=[80.5,139.5]; pwr=[79.5,154.5];
holes=[[6, 6], [86.2, 6], [6, 70], [86.2, 70], [86.2, 112.0]]; top_holes=[[6, 6], [86.2, 6], [6, 70], [86.2, 70]];
function cx(i)=8+pitch/2+i*pitch; function cy(j)=8+pitch/2+j*pitch;
module sw(x,y){ translate([x-cut/2,y-cut/2,-1]) cube([cut,cut,T+2]);
  translate([x-pocket/2,y-pocket/2,-1]) cube([pocket,pocket,T-clip+1]); }
module box(c,s,z0,z1){ translate([c[0]-s[0]/2,c[1]-s[1]/2,z0]) cube([s[0],s[1],z1-z0]); }
difference(){
  union(){
    linear_extrude(T) offset(r=r) offset(delta=-r) square([W,H]);
    // raised strip over the pots so the nut clamps 2 mm of plastic above the can
    translate([cx(0)-10.4,pot_y-10.4,0]) linear_extrude(strip_top) offset(r=2) offset(delta=-2)
      square([W-3-(cx(0)-10.4),20.8]);
  }
  for(i=[0:3]) for(j=[0:3]) sw(cx(i),cy(j));
  translate([7,7,-1]) cube([4*pitch+2,4*pitch+2,T-clip+1]);          // diode clearance
  sw(shift[0],shift[1]);
  // pots: hole, can pocket, base-tab clearance
  for(i=[0:3]) translate([cx(i),pot_y,0]){
    translate([0,0,-1]) cylinder(d=pot_hole,h=strip_top+2,$fn=48);
    translate([0,0,-1]) cylinder(d=pot_body_d,h=pot_top+1,$fn=72);
  }
  // 1 mm underside relief over the whole top section (module solder joints, pot tabs), solid around screws
  difference(){ translate([3,8+4*pitch,-1]) cube([W-6,H-3-(8+4*pitch),2]);
    for(p=holes) translate([p[0],p[1],-2]) cylinder(r=4.5,h=5,$fn=32); }
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
  for(p=top_holes){ top = T;
    translate([p[0],p[1],0]){ translate([0,0,-1]) cylinder(d=3.4,h=top+2,$fn=24);
    translate([0,0,top-1.75]) cylinder(d1=3.4,d2=6.6,h=1.76,$fn=32); } }
}
