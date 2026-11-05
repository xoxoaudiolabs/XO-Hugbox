// XO-Hugbox interactive build guide: scene, step engine, hover/click labels.
import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { RoomEnvironment } from 'three/addons/environments/RoomEnvironment.js';

const M = window.buildHugboxModel(THREE);
const G = window.HUGBOX_GUIDE;
const { parts, root, FP, PADNAMES } = M;
const { screenMat } = M.buildModel(G.STEP);
const $ = (id) => document.getElementById(id);

// ---------------------------------------------------------------- renderer / scene
const view = $('view');
const renderer = new THREE.WebGLRenderer({ antialias: true, preserveDrawingBuffer: true });
renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
renderer.toneMapping = THREE.ACESFilmicToneMapping;
renderer.toneMappingExposure = 1.05;
view.appendChild(renderer.domElement);
const scene = new THREE.Scene();
scene.background = new THREE.Color('#0b0b0b');   // XOXO black
const pmrem = new THREE.PMREMGenerator(renderer);
scene.environment = pmrem.fromScene(new RoomEnvironment(), 0.04).texture;
scene.add(new THREE.HemisphereLight('#ffffff', '#40444c', 0.6));
const sun = new THREE.DirectionalLight('#ffffff', 1.4); sun.position.set(80, -60, 200); scene.add(sun);
const under = new THREE.DirectionalLight('#ffffff', 0.6); under.position.set(-40, 60, -150); scene.add(under);
scene.add(root);
// ground grid for scale (10 mm)
const grid = new THREE.GridHelper(300, 30, '#2a2a28', '#1a1a19'); grid.rotateX(Math.PI / 2); grid.position.set(46, 84, -0.05); scene.add(grid);

const camera = new THREE.PerspectiveCamera(35, 1, 1, 3000);
camera.up.set(0, 0, 1);
camera.position.set(150, -95, 150);
const controls = new OrbitControls(camera, renderer.domElement);
controls.target.set(46, 84, 18);
controls.enableDamping = true;
controls.dampingFactor = 0.12;

function resize() {
  const r = view.getBoundingClientRect();
  renderer.setSize(r.width, r.height, false);
  camera.aspect = r.width / Math.max(1, r.height);
  camera.updateProjectionMatrix();
}
new ResizeObserver(resize).observe(view);

// ---------------------------------------------------------------- part state
const ACCENT = new THREE.Color('#ff3b1f');   // XOXO signal red: parts in this step
function setState(p, state) {
  p.state = state;
  p.group.visible = state !== 'hidden';
  for (const m of p.mats.values()) {
    const b = m.userData.base;
    if (state === 'ghost') { m.transparent = true; m.opacity = 0.08; m.depthWrite = false; }
    else { m.transparent = b.transparent; m.opacity = b.opacity; m.depthWrite = true; }
    if (m.emissive) m.emissive.setHex(b.emissive);
    m.needsUpdate = true;
  }
}

let explode = 0, explodeTarget = 0;
let stepIdx = 0;
const offsetNow = new Map(), offsetTarget = new Map();
for (const id of parts.keys()) { offsetNow.set(id, 0); offsetTarget.set(id, 0); }

// ---------------------------------------------------------------- camera tween
let tween = null;
const DIST = 1.65;   // presets are written close; scale them out from the target
const presetPos = (pos, tgt) => new THREE.Vector3(...pos).sub(new THREE.Vector3(...tgt)).multiplyScalar(DIST).add(new THREE.Vector3(...tgt));
function flyTo([pos, tgt], ms = 900) {
  tween = { t0: performance.now(), ms, p0: camera.position.clone(), t0v: controls.target.clone(),
    p1: presetPos(pos, tgt), t1: new THREE.Vector3(...tgt) };
}
const ease = (t) => (t < 0.5 ? 4 * t * t * t : 1 - Math.pow(-2 * t + 2, 3) / 2);

// ---------------------------------------------------------------- steps UI
const steps = G.steps;
const stepList = $('stepList');
steps.forEach((s, i) => {
  const b = document.createElement('button');
  b.className = 'dot'; b.title = `${i}. ${s.title}`; b.textContent = i;
  b.addEventListener('click', () => goStep(i));
  stepList.appendChild(b);
});

function goStep(i) {
  stepIdx = Math.max(0, Math.min(steps.length - 1, i));
  const s = steps[stepIdx];
  const focus = new Set(s.focus || []), ghost = new Set(s.ghost || []), hide = new Set(s.hide || []);
  for (const p of parts.values()) {
    let st;
    if (s.all) st = 'normal';
    else if (hide.has(p.id)) st = 'hidden';
    else if (p.step > stepIdx) st = 'hidden';
    else if (ghost.has(p.id)) st = 'ghost';
    else if (focus.has(p.id)) st = 'focus';
    else st = 'normal';
    setState(p, st);
    offsetTarget.set(p.id, (s.offsets && s.offsets[p.id]) || 0);
  }
  screenMat.color.set(s.screen || s.all ? '#ffffff' : '#000000');
  explodeTarget = s.explode || 0;
  $('explode').value = explodeTarget;
  flyTo(s.view);
  // text
  $('stepNum').textContent = stepIdx === 0 ? 'Start' : `Step ${stepIdx} of ${steps.length - 1}`;
  $('stepTitle').textContent = s.title;
  const chips = [...focus].map((id) => parts.get(id)).filter(Boolean)
    .map((p) => `<button class="chip" data-part="${p.id}">${p.qty > 1 ? p.qty + '× ' : ''}${p.name}</button>`).join('');
  $('stepBody').innerHTML = (chips ? `<div class="chips">${chips}</div>` : '') + s.body;
  $('stepBody').querySelectorAll('.chip').forEach((c) => c.addEventListener('click', () => select(c.dataset.part)));
  $('stepBody').scrollTop = 0;
  [...stepList.children].forEach((b, k) => { b.classList.toggle('on', k === stepIdx); b.classList.toggle('done', k < stepIdx); });
  $('prev').disabled = stepIdx === 0;
  $('next').disabled = stepIdx === steps.length - 1;
  select(null);
  try { localStorage.setItem('hugbox-guide-step', String(stepIdx)); } catch (e) {}
}
$('prev').addEventListener('click', () => goStep(stepIdx - 1));
$('next').addEventListener('click', () => goStep(stepIdx + 1));
window.addEventListener('keydown', (e) => {
  if (e.target.tagName === 'INPUT') return;
  if (e.key === 'ArrowRight') goStep(stepIdx + 1);
  if (e.key === 'ArrowLeft') goStep(stepIdx - 1);
});
$('explode').addEventListener('input', (e) => { explodeTarget = +e.target.value; });
$('xray').addEventListener('change', (e) => {
  for (const id of ['shell', 'plate']) { const p = parts.get(id); if (p.state !== 'hidden') setState(p, e.target.checked ? 'ghost' : 'normal'); }
});
$('viewTop').addEventListener('click', () => flyTo([[46, 70, 260], [46, 84, 15]]));
$('viewBottom').addEventListener('click', () => flyTo([[46, 70, -250], [46, 84, 15]]));
$('viewIso').addEventListener('click', () => flyTo(steps[0].view));

// ---------------------------------------------------------------- hover + click
const ray = new THREE.Raycaster();
const mouse = new THREE.Vector2();
const tip = $('tip');
let hoverId = null, downAt = null;
function pick(ev) {
  const r = renderer.domElement.getBoundingClientRect();
  mouse.set(((ev.clientX - r.left) / r.width) * 2 - 1, -((ev.clientY - r.top) / r.height) * 2 + 1);
  ray.setFromCamera(mouse, camera);
  const targets = [];
  for (const p of parts.values()) if (p.group.visible && p.state !== 'ghost') targets.push(p.group);
  const hit = ray.intersectObjects(targets, true).find((h) => h.object.visible);
  return hit ? hit.object.userData.part : null;
}
renderer.domElement.addEventListener('pointermove', (ev) => {
  const id = pick(ev);
  hoverId = id;
  if (id) {
    const p = parts.get(id);
    tip.style.display = 'block';
    tip.innerHTML = `<b>${p.qty > 1 ? p.qty + '× ' : ''}${p.name}</b><span>click for details</span>`;
    const r = view.getBoundingClientRect();
    tip.style.left = Math.min(ev.clientX - r.left + 14, r.width - 260) + 'px';
    tip.style.top = (ev.clientY - r.top + 14) + 'px';
    renderer.domElement.style.cursor = 'pointer';
  } else { tip.style.display = 'none'; renderer.domElement.style.cursor = ''; }
});
renderer.domElement.addEventListener('pointerleave', () => { tip.style.display = 'none'; hoverId = null; });
renderer.domElement.addEventListener('pointerdown', (ev) => { downAt = [ev.clientX, ev.clientY]; });
renderer.domElement.addEventListener('pointerup', (ev) => {
  if (!downAt || Math.hypot(ev.clientX - downAt[0], ev.clientY - downAt[1]) > 4) return;
  select(pick(ev));
});

let selected = null;
function select(id) {
  selected = id;
  const card = $('partCard');
  if (!id) { card.style.display = 'none'; return; }
  const p = parts.get(id);
  let html = `<div class="pc-h"><b>${p.qty > 1 ? p.qty + '× ' : ''}${p.name}</b><button id="pcClose" aria-label="Close">×</button></div><p>${p.info}</p>`;
  if (p.refs.length && p.refs.length <= 2) {
    for (const ref of p.refs) {
      const f = FP[ref]; if (!f) continue;
      const names = PADNAMES[ref];
      html += `<div class="pins"><div class="ref">${ref}${f.back ? ' (back)' : ''}</div>` + f.pads.filter((q) => !q.np).map((q, i) =>
        `<span><i>${names ? names[i] : 'pin ' + q.n}</i>${q.net || '-'}</span>`).join('') + '</div>';
    }
  } else if (p.refs.length) html += `<p class="dim">Refs: ${p.refs[0]}–${p.refs[p.refs.length - 1]}</p>`;
  html += `<p class="dim">Installed in step ${p.step === 0 ? '-' : p.step}: ${steps[p.step] ? steps[p.step].title : ''}</p>`;
  card.innerHTML = html;
  card.style.display = 'block';
  $('pcClose').addEventListener('click', () => select(null));
}

// ---------------------------------------------------------------- floating labels for the step's parts
const labelLayer = $('labels');
const labelEls = new Map();
const box = new THREE.Box3(), v = new THREE.Vector3();
function updateLabels() {
  const s = steps[stepIdx];
  const want = new Set(s.all ? [] : (s.focus || []));
  if (selected) want.add(selected);
  for (const [id, el] of labelEls) if (!want.has(id)) { el.remove(); labelEls.delete(id); }
  const r = view.getBoundingClientRect();
  for (const id of want) {
    const p = parts.get(id);
    if (!p || !p.group.visible) continue;
    let el = labelEls.get(id);
    if (!el) { el = document.createElement('div'); el.className = 'lbl'; el.textContent = p.name; labelLayer.appendChild(el); labelEls.set(id, el); }
    box.setFromObject(p.group);
    box.getCenter(v); v.z = box.max.z;
    v.project(camera);
    const behind = v.z > 1;
    el.style.display = behind ? 'none' : 'block';
    el.style.transform = `translate(${(v.x * 0.5 + 0.5) * r.width}px, ${(-v.y * 0.5 + 0.5) * r.height}px) translate(-50%, -130%)`;
  }
}

// ---------------------------------------------------------------- loop
function frame(t) {
  requestAnimationFrame(frame);
  explode += (explodeTarget - explode) * 0.12;
  for (const p of parts.values()) {
    const o = offsetNow.get(p.id) + (offsetTarget.get(p.id) - offsetNow.get(p.id)) * 0.1;
    offsetNow.set(p.id, o);
    p.group.position.z = explode * p.explode + o;
    if (p.state === 'focus') {
      const k = (0.12 + 0.09 * Math.sin(t / 260)) * (p.xray ? 0.35 : 1);   // big printed parts glow less
      for (const m of p.mats.values()) if (m.emissive) m.emissive.copy(m.color).lerp(ACCENT, 0.35).multiplyScalar(k * 2.2);   // brighten, keep the part's hue
    }
  }
  if (tween) {
    const k = Math.min(1, (t - tween.t0) / tween.ms), e = ease(k);
    camera.position.lerpVectors(tween.p0, tween.p1, e);
    controls.target.lerpVectors(tween.t0v, tween.t1, e);
    if (k >= 1) tween = null;
  }
  controls.update();
  renderer.render(scene, camera);
  updateLabels();
}

resize();
let start = 0;
try { start = +(localStorage.getItem('hugbox-guide-step') || 0); } catch (e) {}
const hashStep = /step=(\d+)/.exec(location.hash);
if (hashStep) start = +hashStep[1];
goStep(start || 0);
camera.position.copy(presetPos(...steps[stepIdx].view)); controls.target.set(...steps[stepIdx].view[1]); tween = null;
requestAnimationFrame(frame);
$('loading').remove();
window.hugboxGuide = { goStep, parts, camera, controls, select };
