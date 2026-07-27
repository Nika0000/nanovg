// nanovg playground -- application shell.
//
// Wires the editor to the wasm renderer: compile on idle, run inside the frame
// loop, surface throws on the offending line, and mirror the backend's draw-call
// counters so the batching behaviour is visible while you type.

import { createContext } from './nvg.js';
import { createEditor } from './editor.js';
import { SNIPPETS, DEFAULT_SNIPPET, findSnippet } from './snippets.js';
import { encodeState, decodeState } from './share.js';
import { buildEntries } from './complete.js';

const RECOMPILE_DELAY = 300;
const MAX_CONSECUTIVE_ERRORS = 3;
// Matches the breakpoint in style.css where .layout switches from side-by-side
// columns to stacked rows -- the splitter needs to know which axis it drags.
const NARROW_QUERY = window.matchMedia('(max-width: 900px)');

const el = (id) => document.getElementById(id);

const dom = {
  canvas: el('canvas'),
  stage: el('stage'),
  editor: el('editor'),
  layout: el('layout'),
  split: el('split'),
  snippetMenu: el('snippet-menu'),
  snippetBlurb: el('snippet-blurb'),
  runBtn: el('run'),
  runLabel: el('run-label'),
  shareBtn: el('share'),
  helpBtn: el('help'),
  helpPanel: el('help-panel'),
  status: el('status'),
  fps: el('fps'),
  console: el('console'),
  inspector: el('inspector'),
  inspectorToggle: el('inspector-toggle'),
  consoleToggle: el('console-toggle'),
  stats: el('stats'),
  apiList: el('api-list'),
  apiFilter: el('api-filter'),
  ghStarCount: el('gh-star-count'),
};

const state = {
  ctx: null,
  draw: null,          // compiled sketch
  cache: {},           // sketch-owned scratch, reset on recompile
  sharedCache: {},     // preloaded assets, merged into cache on every compile
  running: true,
  errorStreak: 0,
  startTime: 0,
  frame: 0,
  lastFrameTime: 0,
  fpsAccum: 0,
  fpsFrames: 0,
  mouse: { x: 0, y: 0, down: false, inside: false },
  dirty: false,
  compileTimer: 0,
};

// --- console ----------------------------------------------------------------

function log(kind, message, { line } = {}) {
  const row = document.createElement('div');
  row.className = `log log-${kind}`;
  if (line) {
    const link = document.createElement('button');
    link.className = 'log-line';
    link.textContent = `line ${line}`;
    link.addEventListener('click', () => editor.revealLine(line));
    row.appendChild(link);
  }
  row.appendChild(document.createTextNode(message));
  dom.console.appendChild(row);
  dom.console.scrollTop = dom.console.scrollHeight;
  while (dom.console.children.length > 60) dom.console.firstChild.remove();
}

const clearConsole = () => { dom.console.innerHTML = ''; };

function setStatus(text, kind = 'ok') {
  dom.status.textContent = text;
  dom.status.dataset.kind = kind;
}

// --- error locating ---------------------------------------------------------

// Sketch source is wrapped in a function, so reported line numbers are offset
// by however many lines the engine counts for the generated header. Rather than
// hardcode a per-engine guess, measure it once: compile a sketch that throws on
// its own first line and see what the stack claims.
const SKETCH_URL = 'nanovg-sketch';
const SKETCH_FRAME = new RegExp(`${SKETCH_URL}[^\\s)]*:(\\d+):(\\d+)`);

let wrapperLineOffset = 1;

function calibrateLineOffset() {
  try {
    // eslint-disable-next-line no-new-func
    new Function(`throw new Error('probe');\n//# sourceURL=${SKETCH_URL}.js`)();
  } catch (err) {
    const m = SKETCH_FRAME.exec(String(err.stack || ''));
    if (m) wrapperLineOffset = parseInt(m[1], 10) - 1;
  }
}

function locateError(err) {
  const m = SKETCH_FRAME.exec(String(err && err.stack || ''));
  if (!m) return null;
  const line = parseInt(m[1], 10) - wrapperLineOffset;
  return line > 0 ? { line, column: parseInt(m[2], 10) } : null;
}

// --- compile ----------------------------------------------------------------

function compile(source) {
  clearConsole();
  editor.setMarkers([]);

  let fn;
  try {
    // eslint-disable-next-line no-new-func
    fn = new Function(
      'nvg', 't', 'dt', 'frame', 'w', 'h', 'mouse', 'cache',
      `${source}\n//# sourceURL=${SKETCH_URL}.js`,
    );
  } catch (err) {
    // A syntax error has no usable stack; report it without a line marker
    // rather than guessing one.
    setStatus('syntax error', 'error');
    log('error', `${err.name}: ${err.message}`);
    state.draw = null;
    return false;
  }

  state.draw = fn;
  // Fresh scratch per compile, but the preloaded assets survive: snippets
  // reference cache.img and shouldn't have to re-fetch it on every keystroke.
  state.cache = { ...state.sharedCache };
  state.errorStreak = 0;
  state.startTime = performance.now();
  state.frame = 0;
  if (!state.running) setRunning(true);
  setStatus('running', 'ok');
  return true;
}

function scheduleCompile(source) {
  clearTimeout(state.compileTimer);
  setStatus('editing…', 'pending');
  state.compileTimer = setTimeout(() => compile(source), RECOMPILE_DELAY);
}

// --- frame loop -------------------------------------------------------------

function resize() {
  const dpr = Math.min(window.devicePixelRatio || 1, 2);
  const rect = dom.stage.getBoundingClientRect();
  const w = Math.max(1, Math.floor(rect.width));
  const h = Math.max(1, Math.floor(rect.height));
  dom.canvas.style.width = `${w}px`;
  dom.canvas.style.height = `${h}px`;
  dom.canvas.width = Math.floor(w * dpr);
  dom.canvas.height = Math.floor(h * dpr);
  return { w, h, dpr };
}

// --- pane splitter ------------------------------------------------------------
// Drags the editor/preview boundary via grid-template-columns (or -rows once
// stacked below 900px). Written as a fraction of the layout box rather than a
// pixel offset so it stays correct across that breakpoint and window resizes.

const SPLIT_MIN = 0.2;
const SPLIT_MAX = 0.8;
let splitFrac = 0.5;

function applySplit() {
  const pct = `${(splitFrac * 100).toFixed(3)}%`;
  // minmax() keeps the same floor the static CSS had, so dragging can't
  // squeeze either pane below a usable width/height.
  dom.layout.style.gridTemplateColumns = NARROW_QUERY.matches
    ? '' : `minmax(320px, ${pct}) 1px minmax(360px, 1fr)`;
  dom.layout.style.gridTemplateRows = NARROW_QUERY.matches
    ? `minmax(120px, ${pct}) 1px minmax(160px, 1fr)` : '';
  dom.split.setAttribute('aria-orientation', NARROW_QUERY.matches ? 'horizontal' : 'vertical');
  dom.split.setAttribute('aria-valuenow', String(Math.round(splitFrac * 100)));
}

function setSplit(frac) {
  splitFrac = Math.min(SPLIT_MAX, Math.max(SPLIT_MIN, frac));
  applySplit();
}

function bindSplitter() {
  let dragging = false;

  const fracFromEvent = (ev) => {
    const rect = dom.layout.getBoundingClientRect();
    return NARROW_QUERY.matches
      ? (ev.clientY - rect.top) / rect.height
      : (ev.clientX - rect.left) / rect.width;
  };

  dom.split.addEventListener('pointerdown', (ev) => {
    dragging = true;
    dom.split.setPointerCapture(ev.pointerId);
    document.body.classList.add(NARROW_QUERY.matches ? 'is-resizing-row' : 'is-resizing-col');
  });
  dom.split.addEventListener('pointermove', (ev) => {
    if (!dragging) return;
    setSplit(fracFromEvent(ev));
    handleStageResize();
  });
  const stopDrag = (ev) => {
    if (!dragging) return;
    dragging = false;
    dom.split.releasePointerCapture(ev.pointerId);
    document.body.classList.remove('is-resizing-row', 'is-resizing-col');
  };
  dom.split.addEventListener('pointerup', stopDrag);
  dom.split.addEventListener('pointercancel', stopDrag);

  // Keyboard equivalent for anyone who can't drag: arrow keys nudge by 2%.
  dom.split.addEventListener('keydown', (ev) => {
    const horizontal = !NARROW_QUERY.matches;
    const key = horizontal ? { dec: 'ArrowLeft', inc: 'ArrowRight' } : { dec: 'ArrowUp', inc: 'ArrowDown' };
    if (ev.key !== key.dec && ev.key !== key.inc) return;
    ev.preventDefault();
    setSplit(splitFrac + (ev.key === key.inc ? 0.02 : -0.02));
    handleStageResize();
  });

  NARROW_QUERY.addEventListener('change', applySplit);
}

// --- collapsible panels -------------------------------------------------------
// Both the frame inspector and the console are secondary to the canvas/editor;
// collapsing hides their content but keeps the header visible so they stay
// reachable. Neither collapse changes the canvas size on its own, but the
// console one shrinks body's grid row, which does -- hence the resize.

function bindCollapsible(toggleBtn, { onToggle }) {
  toggleBtn.addEventListener('click', () => {
    const expanded = toggleBtn.getAttribute('aria-expanded') !== 'false';
    toggleBtn.setAttribute('aria-expanded', String(!expanded));
    toggleBtn.title = expanded ? 'Expand' : 'Collapse';
    onToggle(expanded);
  });
}

function bindPanelToggles() {
  bindCollapsible(dom.inspectorToggle, {
    onToggle: (collapsed) => dom.inspector.classList.toggle('is-collapsed', collapsed),
  });
  bindCollapsible(dom.consoleToggle, {
    onToggle: (collapsed) => {
      document.body.classList.toggle('console-collapsed', collapsed);
      handleStageResize();
    },
  });
}

function renderStats(stats) {
  const rows = [
    ['draw calls', stats.calls],
    ['vertices', stats.verts],
    ['paths', stats.paths],
    ['uniform blocks', stats.uniforms],
    ['textures', stats.textures],
  ];
  const kinds = [
    ['fill (stencil+cover)', stats.fill],
    ['convex fill', stats.convexFill],
    ['stroke', stats.stroke],
    ['triangles (text)', stats.triangles],
  ].filter(([, v]) => v > 0);

  dom.stats.innerHTML =
    rows.map(([k, v]) => `<div class="stat"><span>${k}</span><b>${v}</b></div>`).join('') +
    (kinds.length
      ? `<div class="stat-sep">call breakdown</div>` +
        kinds.map(([k, v]) => `<div class="stat stat-sub"><span>${k}</span><b>${v}</b></div>`).join('')
      : '');
}

function renderFrame(t, dt) {
  const { w, h, dpr } = resize();
  state.ctx.beginFrame(w, h, dpr);
  try {
    state.draw(state.ctx.nvg, t, dt, state.frame, w, h, state.mouse, state.cache);
    state.errorStreak = 0;
  } catch (err) {
    const at = locateError(err);
    log('error', `${err.name}: ${err.message}`, at || {});
    if (at) editor.setMarkers([{ line: at.line, message: `${err.name}: ${err.message}` }]);
    state.errorStreak++;
    if (state.errorStreak >= MAX_CONSECUTIVE_ERRORS) {
      setRunning(false);
      setStatus('paused after error', 'error');
      log('info', 'Loop paused. Fix the sketch or press Run to resume.');
    }
    // Still flush: a partial frame is more informative than a frozen one.
  }
  renderStats(state.ctx.stats());
  state.ctx.endFrame();
}

function frame(now) {
  requestAnimationFrame(frame);
  if (!state.ctx) return;

  const dt = state.lastFrameTime ? (now - state.lastFrameTime) / 1000 : 0;
  state.lastFrameTime = now;

  state.fpsAccum += dt;
  state.fpsFrames++;
  if (state.fpsAccum >= 0.5) {
    dom.fps.textContent = `${Math.round(state.fpsFrames / state.fpsAccum)} fps`;
    state.fpsAccum = 0;
    state.fpsFrames = 0;
  }

  if (!state.running || !state.draw) return;

  renderFrame((now - state.startTime) / 1000, dt);
  state.frame++;
}

// Canvas backing-buffer size follows the #stage element, which changes on
// window resize, the pane splitter being dragged, and the narrow-layout
// breakpoint flipping editor/preview from side-by-side to stacked -- a
// ResizeObserver catches all three, unlike a 'resize' listener on window
// alone. The rAF loop already resizes+repaints every tick while running; while
// paused nothing else touches the canvas, so force one repaint at the frozen
// time or a resize would leave it stretched or blank.
function handleStageResize() {
  if (!state.ctx) return;
  if (state.draw && !state.running) {
    renderFrame((state.lastFrameTime - state.startTime) / 1000, 0);
  } else {
    resize();
  }
}

function setRunning(on) {
  state.running = on;
  dom.runLabel.textContent = on ? 'Pause' : 'Run';
  dom.runBtn.dataset.state = on ? 'running' : 'paused';
  dom.runBtn.title = on ? 'Pause' : 'Ctrl/Cmd + Enter';
  if (on) {
    // Don't let paused wall-clock time jump the animation forward.
    state.startTime = performance.now() - state.frame * 16;
    setStatus('running', 'ok');
  } else {
    setStatus('paused', 'pending');
  }
}

// --- snippets & sharing -----------------------------------------------------

function populateSnippets() {
  dom.snippetMenu.innerHTML = SNIPPETS
    .map((s) => `<option value="${s.id}">${s.title}</option>`)
    .join('');
}

function loadSnippet(id, { updateHash = true } = {}) {
  const snippet = findSnippet(id);
  if (!snippet) return false;
  dom.snippetMenu.value = id;
  dom.snippetBlurb.textContent = snippet.blurb;
  editor.setValue(snippet.code);
  compile(snippet.code);
  if (updateHash) history.replaceState(null, '', `#snippet=${id}`);
  return true;
}

async function share() {
  const hash = await encodeState(editor.value);
  const url = `${location.origin}${location.pathname}#${hash}`;
  history.replaceState(null, '', `#${hash}`);
  try {
    await navigator.clipboard.writeText(url);
    log('info', 'Shareable link copied to clipboard.');
  } catch {
    log('info', `Shareable link: ${url}`);
  }
}

// --- API reference ----------------------------------------------------------

function renderApi(docs, filter = '') {
  const q = filter.trim().toLowerCase();
  const matches = docs.filter(
    (d) => !q || d.js.toLowerCase().includes(q) || d.name.toLowerCase().includes(q)
      || (d.doc || '').toLowerCase().includes(q),
  );

  const bySection = new Map();
  matches.forEach((d) => {
    const key = d.section || 'Misc';
    if (!bySection.has(key)) bySection.set(key, []);
    bySection.get(key).push(d);
  });

  const esc = (s) => String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;');
  let html = '';
  for (const [section, items] of bySection) {
    html += `<div class="api-section">${esc(section)}</div>`;
    for (const d of items) {
      html += `<details class="api-item"><summary><code>nvg.${esc(d.js)}(${
        d.args.map(esc).join(', ')})</code></summary>`;
      if (d.doc) html += `<p>${esc(d.doc)}</p>`;
      html += `<code class="api-c">${esc(d.signature)}</code></details>`;
    }
  }
  dom.apiList.innerHTML = html || '<div class="api-empty">No matches.</div>';
}

// --- github star --------------------------------------------------------

function formatStarCount(n) {
  if (n < 1000) return String(n);
  return `${(n / 1000).toFixed(n < 10000 ? 1 : 0)}k`;
}

async function loadStarCount() {
  try {
    const res = await fetch('https://api.github.com/repos/Nika0000/nanovg');
    if (!res.ok) return;
    const data = await res.json();
    if (typeof data.stargazers_count === 'number') {
      dom.ghStarCount.textContent = formatStarCount(data.stargazers_count);
    }
  } catch {
    // Offline or rate-limited: leave the count blank, the link still works.
  }
}

// --- boot -------------------------------------------------------------------

const editor = createEditor(dom.editor, {
  onChange: scheduleCompile,
  onRun: (source) => { clearTimeout(state.compileTimer); compile(source); },
});

function bindUi() {
  populateSnippets();
  bindSplitter();
  applySplit();
  bindPanelToggles();

  dom.snippetMenu.addEventListener('change', (ev) => loadSnippet(ev.target.value));
  dom.runBtn.addEventListener('click', () => {
    clearTimeout(state.compileTimer);
    if (state.running) setRunning(false);
    else compile(editor.value);
  });
  dom.shareBtn.addEventListener('click', share);
  dom.helpBtn.addEventListener('click', () => {
    dom.helpPanel.hidden = !dom.helpPanel.hidden;
    dom.helpBtn.setAttribute('aria-expanded', String(!dom.helpPanel.hidden));
  });
  dom.apiFilter.addEventListener('input', (ev) => renderApi(state.ctx.docs, ev.target.value));

  const pos = (ev) => {
    const rect = dom.canvas.getBoundingClientRect();
    state.mouse.x = ev.clientX - rect.left;
    state.mouse.y = ev.clientY - rect.top;
  };
  dom.canvas.addEventListener('pointermove', (ev) => { pos(ev); state.mouse.inside = true; });
  dom.canvas.addEventListener('pointerdown', (ev) => { pos(ev); state.mouse.down = true; });
  dom.canvas.addEventListener('pointerleave', () => { state.mouse.inside = false; });
  window.addEventListener('pointerup', () => { state.mouse.down = false; });

  window.addEventListener('keydown', (ev) => {
    if ((ev.ctrlKey || ev.metaKey) && ev.key === 'Enter') {
      ev.preventDefault();
      clearTimeout(state.compileTimer);
      compile(editor.value);
    }
  });
}

async function boot() {
  calibrateLineOffset();
  bindUi();
  loadStarCount();
  setStatus('loading wasm…', 'pending');

  try {
    state.ctx = await createContext('#canvas');
  } catch (err) {
    setStatus('failed to start', 'error');
    log('error', err.message);
    dom.stage.classList.add('stage-failed');
    return;
  }

  renderApi(state.ctx.docs);
  editor.setCompletions(buildEntries(state.ctx.docs, state.ctx.enums));

  // Fonts and the sample image are shared by the gallery; load once up front so
  // sketches can assume 'sans'/'bold'/'icons' and cache.img exist.
  try {
    await Promise.all([
      state.ctx.nvg.loadFont('sans', 'assets/Roboto-Regular.ttf'),
      state.ctx.nvg.loadFont('bold', 'assets/Roboto-Bold.ttf'),
      state.ctx.nvg.loadFont('light', 'assets/Roboto-Light.ttf'),
      state.ctx.nvg.loadFont('icons', 'assets/entypo.ttf'),
    ]);
    state.sharedCache.img = await state.ctx.nvg.loadImage('assets/image1.jpg');
  } catch (err) {
    log('warn', `asset load: ${err.message}`);
  }

  const shared = await decodeState(location.hash);
  if (shared && shared.code) {
    editor.setValue(shared.code);
    dom.snippetBlurb.textContent = 'Loaded from a shared link.';
    compile(shared.code);
  } else if (!loadSnippet(shared && shared.snippet ? shared.snippet : DEFAULT_SNIPPET,
                          { updateHash: false })) {
    loadSnippet(DEFAULT_SNIPPET, { updateHash: false });
  }

  // Covers window resizes, the splitter drag, and the narrow-layout breakpoint
  // -- anything that changes #stage's box, not just the window's.
  new ResizeObserver(handleStageResize).observe(dom.stage);
  requestAnimationFrame(frame);
}

boot();
