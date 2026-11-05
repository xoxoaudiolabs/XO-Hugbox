import layout as L
zP=L.Z_PCB; zPt=zP+L.PCB_T; zPl=L.Z_PLATE_BOT; zTop=zPl+L.PLATE_T
nx0,ny0,nx1,ny1=L.NOTCH; I=L.INSET
outline=[(I,I),(L.W-I,I),(L.W-I,L.H-I),(nx1,L.H-I),(nx1,ny0),(nx0,ny0)]
keys=[(L.cx(i),L.cy(j)) for i in range(4) for j in range(4)]
zb=L.Z_PCB-2.5-1.6
s=f"""// XO-Hugbox - visual assembly (generated). explode=1 for exploded view.
explode=0; show_shell=1; show_plate=1; show_caps=1; show_bottom=1; e1=explode*55; e2=explode*110; e3=explode*150;
$fn=40;
module keycap(c){{ color(c) hull(){{ translate([-9,-9,0]) cube([18,18,0.1]); translate([-6.35,-6.35,7.4]) cube([12.7,12.7,0.1]); }} }}
module mx(x,y,cap){{ translate([x,y,0]){{
  color("#2b2b2b") translate([-7,-7,{zPt}]) cube([14,14,5]);
  color("#333") hull(){{ translate([-7.8,-7.8,{zTop}]) cube([15.6,15.6,0.1]); translate([-5.5,-5.5,{zTop+5.6}]) cube([11,11,0.1]); }}
  if(show_caps) translate([0,0,{zTop+6.6}+e3]) keycap(cap); }} }}
if(show_shell) color("#d9d4c7") import("xo_hugbox_shell.stl");
if(show_bottom) color("#b8bcc2") translate([46-12.5,40-20,2.2]) cube([25,40,10]);
if(show_bottom) translate([{L.SPK_C[0]},{L.SPK_C[1]},0]){{
  color("#1b1b1b") translate([-15.5,-14.1,{zPl+0.8-15.3}]) cube([31,28.2,15.3]);
  color("#1b1b1b") for(s=[-1,1]) hull(){{ translate([-5.5,s*14.1-0.01,{zPl+0.8-7.65-0.75}]) cube([11,0.02,1.5]);
       translate([0,s*{L.SPK_EAR_PITCH/2},{zPl+0.8-7.65-0.75}]) cylinder(d=6.5,h=1.5); }}
}}
translate([0,0,e1]){{
  color("#1f6b3a") translate([0,0,{zP}]) linear_extrude({L.PCB_T}) polygon({[list(p) for p in outline]});
  // XIAO (USB-C to the left wall)
  color("#222") translate([{L.XIAO_C[0]-10.5},{L.XIAO_C[1]-8.9},{zP-2.5-1.2}]) cube([21,17.8,1.2]);
  color("#c0c0c0") translate([{L.XIAO_C[0]-9.2},{L.XIAO_C[1]-7},{zP-2.5-1.2-2.8}]) cube([12,14,2.8]);
  color("#d8d8d8") translate([{L.XIAO_C[0]-11.0},{L.XIAO_C[1]-4.5},{zP-2.5-1.2-3.2}]) cube([7.5,9,3.2]);
  color("#1e5bd6") translate([{L.MAX_C[0]-9.7},{L.MAX_C[1]-9.7},{zb}]) cube([19.4,18.6,1.6]);
  color("#1e5bd6") translate([{L.PCF_ROW[0]-20},{L.PCF_ROW[1]-14},{zb}]) cube([40,15,1.6]);
  color("#7a2a9a") translate([{L.PCM_X1-L.PCM_W},{L.PCM_Y0},{zb}]) cube([{L.PCM_W},{L.PCM_LEN},1.6]);
  color("#111") translate([{L.PCM_X1-7},{L.PCM_Y0+0.5},{zb-6}]) cube([7,11,6]);
  color("#111") translate([{L.PCM_X1-1},{L.JACK_Y},{zb-3}]) rotate([0,90,0]) cylinder(d=6,h=3.2);
  for(k={[list(p) for p in keys]}) color("#e9e2c8") translate([k[0]-3.8,k[1]-9.5,{zP-1.9}]) rotate([0,90,0]) cylinder(d=1.9,h=7.6,$fn=12);
  color("#1e5bd6") translate([{L.OLED_C[0]-L.OLED_MOD[0]/2:.2f},{L.OLED_MOD_Y0:.2f},{zPt+0.8}]) cube([{L.OLED_MOD[0]},{L.OLED_MOD[1]},1.2]);
  color("#050505") translate([{L.OLED_C[0]-L.OLED_GLASS[0]/2:.2f},{L.OLED_GLASS_Y0:.2f},{zPt+2.0}]) cube([{L.OLED_GLASS[0]},{L.OLED_GLASS[1]},1.4]);
  color("#7fd7ff") translate([{L.OLED_C[0]-14},{L.OLED_C[1]+L.OLED_WIN_OFF-6.5},{zPt+3.61}]) cube([28,1.2,0.05]);
  color("#7fd7ff") translate([{L.OLED_C[0]-14},{L.OLED_C[1]+L.OLED_WIN_OFF+3},{zPt+3.61}]) cube([18,3,0.05]);
  for(i=[0:5]) color("#7fd7ff") translate([{L.OLED_C[0]-14}+i*4.8,{L.OLED_C[1]+L.OLED_WIN_OFF-4.5},{zPt+3.61}]) cube([3,1+i*1.2,0.05]);
  // pots - pin tab faces the keys
  for(i=[0:3]) translate([{L.BORDER}+{L.P}/2+i*{L.P},{L.POT_Y},0]){{
    color("#c8c8c8") translate([0,0,{zPt}]) cylinder(d={L.POT_CAN_D},h={L.POT_BODY_H});
    color("#d0d0d0") translate([0,0,{zPt+L.POT_BODY_H}]) cylinder(d=6.65,h=7.33);
    color("#e0e0e0") translate([0,0,{zPt+L.POT_BODY_H+7.33}]) cylinder(d=5.95,h=8.26);
    // straight solder lugs toward the keys, wire legs down into the oval slots
    for(k=[-1:1]){{ color("#d8c27a") translate([k*{L.POT_PITCH}-1,-({L.POT_CAN_D/2+L.POT_LUG_L}),{zPt+L.POT_LUG_Z}]) cube([2,{L.POT_LUG_L+0.5},{L.POT_LUG_T}]);
      color("#bbb") translate([k*{L.POT_PITCH},-({L.POT_CAN_D/2+L.POT_LUG_L-1.2}),{zPt-L.PCB_T-1.2}]) cylinder(d=0.6,h={L.POT_LUG_Z+L.PCB_T+1.2},$fn=8); }}
    if(show_caps) translate([0,0,{zTop+4+2}+e3]){{ color("#2a2a2a") cylinder(d=14.35,h=16.8,$fn=24); color("#bbbbbb") translate([0,0,16.8]) cylinder(d=12,h=0.3); color("white") translate([-0.5,-6,17.1]) cube([1,4.5,0.2]); }}
    if(show_caps) color("#999") translate([0,0,{zTop+4}+e2]) difference(){{ cylinder(d=11,h=2,$fn=6); cylinder(d=7,h=5,center=true); }}
  }}
  color("#c8c8c8") translate([{L.PWR[0]-4.35},{L.PWR[1]-1.85},{zPt}]) cube([8.7,3.7,3.7]);
  color("#111") translate([{L.PWR[0]-1.5},{L.PWR[1]-0.75},{zPt+3.7}]) cube([1.5,1.5,5]);
  for(k={[list(p) for p in keys]}) mx(k[0],k[1],"#ece9e1");
  mx({L.SHIFT[0]},{L.SHIFT[1]},"#e8792b");
}}
if(show_plate) translate([0,0,{zPl}+e2]) color("#f2efe6") import("xo_hugbox_top_plate.stl");
"""
open('assembly.scad','w').write(s)
