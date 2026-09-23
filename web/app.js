// Plays back a trace written by `stuff3d --trace web/trace.js`.
// The screen view draws the exact pixel coordinates the C++ renderer computed;
// nothing here re-projects the model.
(function () {
  const T = window.TRACE;
  const frames = T.frames;
  const $ = (id) => document.getElementById(id);

  // Unique edges from the triangle list (the C++ loop draws shared edges twice;
  // the picture is the same).
  const edges = [];
  const seen = new Set();
  for (const f of T.faces) {
    for (let i = 0; i < f.length; i++) {
      const a = f[i], b = f[(i + 1) % f.length];
      const key = a < b ? a * 4096 + b : b * 4096 + a;
      if (!seen.has(key)) { seen.add(key); edges.push([a, b]); }
    }
  }

  // ---- screen -----------------------------------------------------------
  const sc = $('screen'), sx = sc.getContext('2d');
  function depthColor(z) {
    // near (z ~ 0.3) -> bright green, far (z >= 5) -> dark green
    const t = Math.min(1, Math.max(0, (z - 0.3) / 4.5));
    const g = Math.round(245 - t * 170), r = Math.round(54 - t * 40), b = Math.round(106 - t * 70);
    return `rgb(${r},${g},${b})`;
  }
  function drawScreen(fr) {
    const depth = $('optDepth').checked;
    sx.fillStyle = '#000';
    sx.fillRect(0, 0, 800, 800);
    sx.lineWidth = 1.2;
    const px = fr.px;
    if (!depth) { sx.strokeStyle = '#00ff00'; sx.beginPath(); }
    for (const [a, b] of edges) {
      const p = px[a], q = px[b];
      if (!p || !q) continue;
      if (depth) {
        sx.strokeStyle = depthColor((p[2] + q[2]) / 2);
        sx.beginPath(); sx.moveTo(p[0], p[1]); sx.lineTo(q[0], q[1]); sx.stroke();
      } else { sx.moveTo(p[0], p[1]); sx.lineTo(q[0], q[1]); }
    }
    if (!depth) sx.stroke();

    if ($('optClip').checked && fr.clipped.length) {
      sx.strokeStyle = '#ffb547'; sx.lineWidth = 2;
      sx.beginPath();
      for (const c of fr.clipped) { sx.moveTo(c[0], c[1]); sx.lineTo(c[2], c[3]); }
      sx.stroke();
    }
    if ($('optVerts').checked) {
      sx.fillStyle = '#d6ffe0';
      for (const p of px) if (p) sx.fillRect(p[0] - 1.5, p[1] - 1.5, 3, 3);
    }
    const tp = px[T.tracked];
    if (tp) {
      sx.strokeStyle = '#5fd4ff'; sx.fillStyle = '#5fd4ff'; sx.lineWidth = 2;
      sx.beginPath(); sx.arc(tp[0], tp[1], 7, 0, Math.PI * 2); sx.stroke();
      sx.beginPath(); sx.arc(tp[0], tp[1], 2.5, 0, Math.PI * 2); sx.fill();
      sx.font = '600 15px ui-monospace, Consolas, monospace';
      sx.fillText(`(${tp[0].toFixed(0)}, ${tp[1].toFixed(0)})`, tp[0] + 12, tp[1] - 10);
    }
    sx.font = '14px ui-monospace, Consolas, monospace';
    const drawn = fr.px.filter(Boolean).length;
    sx.fillStyle = 'rgba(0,0,0,.75)';
    sx.fillRect(0, 764, 800, 36);
    sx.fillStyle = 'rgba(214,229,216,.7)';
    sx.fillText(`t=${fr.t.toFixed(2)}s  vertices in front of near plane: ${drawn}/${px.length}` +
      (fr.clipped.length ? `  clipped edges: ${fr.clipped.length}` : ''), 14, 786);
  }

  // ---- top-down map -------------------------------------------------------
  // World X to the right, world Z up the page. The camera trail and heading
  // come straight from the trace; the model outline uses the traced spin angle.
  const mc = $('map'), mx = mc.getContext('2d');
  let minX = -0.8, maxX = 0.8, minZ = -0.3, maxZ = T.dz + 0.8;
  for (const f of frames) {
    minX = Math.min(minX, f.camera[0]); maxX = Math.max(maxX, f.camera[0]);
    minZ = Math.min(minZ, f.camera[2]); maxZ = Math.max(maxZ, f.camera[2]);
  }
  const pad = 0.5; minX -= pad; maxX += pad; minZ -= pad; maxZ += pad;
  const scale = Math.min(mc.width / (maxX - minX), mc.height / (maxZ - minZ));
  const ox = (mc.width - (maxX - minX) * scale) / 2, oz = (mc.height - (maxZ - minZ) * scale) / 2;
  const M = (x, z) => [ox + (x - minX) * scale, mc.height - (oz + (z - minZ) * scale)];

  function drawMap(fi) {
    const fr = frames[fi];
    mx.fillStyle = '#070a08'; mx.fillRect(0, 0, mc.width, mc.height);
    // grid
    mx.strokeStyle = '#141c16'; mx.lineWidth = 1;
    for (let x = Math.ceil(minX); x <= maxX; x++) { const [a] = M(x, 0); mx.beginPath(); mx.moveTo(a, 0); mx.lineTo(a, mc.height); mx.stroke(); }
    for (let z = Math.ceil(minZ); z <= maxZ; z++) { const [, b] = M(0, z); mx.beginPath(); mx.moveTo(0, b); mx.lineTo(mc.width, b); mx.stroke(); }

    // model footprint (x/z of every vertex after spin + push)
    const c = Math.cos(fr.angle), s = Math.sin(fr.angle);
    mx.fillStyle = 'rgba(54,245,106,.55)';
    for (const v of T.vertices) {
      const wx = v[0] * c - v[2] * s, wz = v[0] * s + v[2] * c + T.dz;
      const [a, b] = M(wx, wz); mx.fillRect(a - 1, b - 1, 2, 2);
    }
    const tw = fr.stages.world, [ta, tb] = M(tw[0], tw[2]);
    mx.fillStyle = '#5fd4ff'; mx.beginPath(); mx.arc(ta, tb, 3.5, 0, 7); mx.fill();

    // camera trail
    mx.strokeStyle = 'rgba(255,181,71,.45)'; mx.lineWidth = 1.5; mx.beginPath();
    for (let i = 0; i <= fi; i++) { const [a, b] = M(frames[i].camera[0], frames[i].camera[2]); i ? mx.lineTo(a, b) : mx.moveTo(a, b); }
    mx.stroke();

    // camera frustum: forward = (sin yaw, cos yaw); x/z in [-1, 1] -> 90 degree FOV
    const [cx, cz] = [fr.camera[0], fr.camera[2]];
    const fx = Math.sin(fr.yaw), fz = Math.cos(fr.yaw), rx = Math.cos(fr.yaw), rz = -Math.sin(fr.yaw);
    const L = 1.6;
    const [p0x, p0y] = M(cx, cz);
    const [p1x, p1y] = M(cx + (fx - rx) * L, cz + (fz - rz) * L);
    const [p2x, p2y] = M(cx + (fx + rx) * L, cz + (fz + rz) * L);
    mx.fillStyle = 'rgba(255,181,71,.10)'; mx.strokeStyle = 'rgba(255,181,71,.6)'; mx.lineWidth = 1;
    mx.beginPath(); mx.moveTo(p0x, p0y); mx.lineTo(p1x, p1y); mx.lineTo(p2x, p2y); mx.closePath(); mx.fill(); mx.stroke();
    const [hx, hy] = M(cx + fx * L, cz + fz * L);
    mx.setLineDash([4, 4]); mx.beginPath(); mx.moveTo(p0x, p0y); mx.lineTo(hx, hy); mx.stroke(); mx.setLineDash([]);
    const n = T.near * 6; // near plane, exaggerated 6x so it is visible
    const [n1x, n1y] = M(cx + (fx - rx) * n, cz + (fz - rz) * n), [n2x, n2y] = M(cx + (fx + rx) * n, cz + (fz + rz) * n);
    mx.strokeStyle = '#ffb547'; mx.lineWidth = 2; mx.beginPath(); mx.moveTo(n1x, n1y); mx.lineTo(n2x, n2y); mx.stroke();
    mx.fillStyle = '#ffb547'; mx.beginPath(); mx.arc(p0x, p0y, 4, 0, 7); mx.fill();

    mx.fillStyle = '#7d917f'; mx.font = '11px ui-monospace, Consolas, monospace';
    mx.fillText(`camera (${cx.toFixed(2)}, ${cz.toFixed(2)})  yaw ${(fr.yaw * 180 / Math.PI).toFixed(0)}°`, 8, 16);
    mx.fillText('+X →', mc.width - 44, mc.height - 8);
    mx.fillText('↑ +Z', 8, mc.height - 8);
  }

  // ---- stages -------------------------------------------------------------
  const f3 = (v) => `(${v.map((n) => n.toFixed(2).padStart(5)).join(', ')})`;
  const f2 = (v) => `(${v.map((n) => n.toFixed(2)).join(', ')})`;
  const STAGES = [
    ['Model space', 'vertex #' + T.tracked + ' from penger.cpp', (s) => f3(s.model)],
    ['Spin', 'rotate_y(p, angle)', (s) => f3(s.spun)],
    ['World', 'translate_z(p, dz = ' + T.dz + ')', (s) => f3(s.world)],
    ['Relative to camera', 'p − camera', (s) => f3(s.relative)],
    ['View space', 'rotate_y(p, yaw)  → camera looks down +Z', (s) => f3(s.view)],
    ['NDC', 'project: (x / z, y / z)', (s) => f2(s.ndc)],
    ['Pixel', 'screen: ((x+1)/2·W, (1−y)/2·H)', (s) => f2(s.pixel)],
  ];
  const list = $('stages');
  list.innerHTML = STAGES.map(([n, op]) =>
    `<li><span class="name">${n}</span><span class="val"></span><span class="op">${op}</span></li>`).join('');
  const lis = [...list.children];
  function drawStages(fr) {
    const s = fr.stages, behind = s.view[2] < T.near;
    STAGES.forEach(([, , fmt], i) => {
      const li = lis[i];
      li.classList.toggle('bad', behind && i >= 5);
      li.querySelector('.val').textContent = behind && i >= 5 ? 'behind camera, not drawn' : fmt(s);
    });
  }

  // ---- transport ----------------------------------------------------------
  const segs = [];
  frames.forEach((f, i) => {
    const last = segs[segs.length - 1];
    if (last && last.keys === f.keys) last.end = i; else segs.push({ keys: f.keys, start: i, end: i });
  });
  $('segments').innerHTML = segs.map((s) =>
    `<div style="flex:${s.end - s.start + 1}">${s.keys ? s.keys.toUpperCase().split('').join('+') : '·'}</div>`).join('');
  const segEls = [...$('segments').children];
  const kbds = [...document.querySelectorAll('kbd')];
  const scrub = $('scrub');
  scrub.max = frames.length - 1;

  let idx = 0, playing = true, acc = 0, lastTs = null;
  function render() {
    const fr = frames[idx];
    drawScreen(fr); drawMap(idx); drawStages(fr);
    scrub.value = idx;
    $('time').textContent = fr.t.toFixed(2) + ' s';
    kbds.forEach((k) => k.classList.toggle('on', fr.keys.includes(k.dataset.k)));
    segs.forEach((s, i) => segEls[i].classList.toggle('cur', idx >= s.start && idx <= s.end));
  }
  function tick(ts) {
    if (lastTs !== null && playing) {
      acc += (ts - lastTs) / 1000 * T.fps * parseFloat($('speed').value);
      while (acc >= 1) { acc -= 1; idx = (idx + 1) % frames.length; }
      render();
    }
    lastTs = ts;
    requestAnimationFrame(tick);
  }
  $('play').onclick = () => { playing = !playing; $('play').textContent = playing ? 'Pause' : 'Play'; };
  scrub.oninput = () => { idx = +scrub.value; render(); };
  ['optDepth', 'optVerts', 'optClip'].forEach((id) => { $(id).onchange = render; });
  document.addEventListener('keydown', (e) => {
    if (e.code === 'Space') { e.preventDefault(); $('play').click(); }
    if (e.code === 'ArrowRight') { idx = Math.min(frames.length - 1, idx + 1); render(); }
    if (e.code === 'ArrowLeft') { idx = Math.max(0, idx - 1); render(); }
  });
  // allow ?t=8.2&pause for screenshots
  const q = new URLSearchParams(location.search);
  if (q.has('t')) idx = Math.max(0, frames.findIndex((f) => f.t >= parseFloat(q.get('t'))));
  if (q.has('pause')) { playing = false; $('play').textContent = 'Play'; }
  render();
  requestAnimationFrame(tick);
})();
