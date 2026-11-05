#!/usr/bin/env bash
# Rebuild sim/src/hugbox_core.js (WASM embedded, works from file://) from the shared firmware library
# firmware/lib/hugbox_core (the same C++ the board runs) + sim/wasm_main.cpp + U8g2.
# Run in WSL Ubuntu (emscripten from apt):
#   wsl -d Ubuntu -- bash -c "cd '<XO-Hugbox>/sim' && ./build_wasm.sh"
# U8g2 comes from the PlatformIO lib cache the firmware build already downloaded; if it is missing,
# build the firmware once (pio run -e hugbox) or point U8G2 at another copy of U8g2/src.
set -euo pipefail
cd "$(dirname "$0")"
CORE=../firmware/lib/hugbox_core
U8G2=${U8G2:-}
if [ -z "$U8G2" ]; then
  for d in ../firmware/.pio/libdeps/hugbox/U8g2/src ../firmware/.pio/libdeps/bringup/U8g2/src; do
    [ -f "$d/clib/u8g2.h" ] && { U8G2=$d; break; }
  done
fi
[ -f "$U8G2/clib/u8g2.h" ] || { echo "U8g2 not found (looked in firmware/.pio/libdeps/*/U8g2/src)"; exit 1; }
OBJ=${OBJ:-/tmp/hugbox_u8g2_obj}; mkdir -p "$OBJ"
# compile U8g2 once (C, ~200 files incl. the big font table); the linker keeps only what is used
objs=()
for c in "$U8G2"/clib/u8g2_*.c "$U8G2"/clib/u8x8_*.c "$U8G2"/clib/u8log*.c; do
  o="$OBJ/$(basename "${c%.c}").o"; objs+=("$o")
  [ "$o" -nt "$c" ] || emcc -O2 -c "$c" -I"$U8G2/clib" -o "$o"
done
emcc -O2 -std=c++17 -Wall -I"$U8G2" -I"$U8G2/clib" -I"$CORE" \
  "$CORE"/bringup_app.cpp "$CORE"/model.cpp "$CORE"/main_app.cpp "$CORE"/engine.cpp "$CORE"/kit.cpp "$CORE"/fx.cpp "$CORE"/io.cpp \
  wasm_main.cpp "${objs[@]}" \
  -o src/hugbox_core.js \
  -sSINGLE_FILE=1 -sMODULARIZE=1 -sEXPORT_NAME=createHugboxCore -sENVIRONMENT=web,node \
  -sEXPORTED_FUNCTIONS=_hb_init,_hb_set_inputs,_hb_loop,_hb_render,_hb_panel,_hb_frames,_hb_tone_hz,_hb_tone_gain,_hb_app,_hb_status,_hb_brightness,_hb_set_param,_hb_param,_hb_pad_note,_hb_knob_param,_hb_param_label,_hb_param_name,_hb_param_text,_hb_view,_hb_track,_hb_midi_in \
  -sEXPORTED_RUNTIME_METHODS=UTF8ToString -sINITIAL_MEMORY=16777216 -sTOTAL_STACK=65536
ls -l src/hugbox_core.js && python3 bundle.py src/index.html index.html
