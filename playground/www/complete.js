// nanovg playground -- completion and signature help.
//
// The point of the playground is learning the API, so completion carries the
// same doc text as the reference panel: pick a member, read what it does, see
// the C signature it maps to. Data comes from the generated DOCS index, so it
// can't drift from the header.
//
// No dependency on a language service: sketches are short and the interesting
// surface is one object deep (`nvg.`), which a few hundred lines of matching
// handles better than a generic JS analyser would.

// Things the shell puts in scope for every sketch. Kept here rather than in
// app.js so the completion list and the help panel can't disagree.
export const SKETCH_GLOBALS = [
  { label: 'nvg', kind: 'var', doc: 'The NanoVG context. Every drawing call hangs off this object.' },
  { label: 't', kind: 'var', doc: 'Seconds since the sketch was compiled. Use for animation.' },
  { label: 'dt', kind: 'var', doc: 'Seconds since the previous frame.' },
  { label: 'frame', kind: 'var', doc: 'Frame counter, reset on recompile.' },
  { label: 'w', kind: 'var', doc: 'Canvas width in points (not pixels).' },
  { label: 'h', kind: 'var', doc: 'Canvas height in points (not pixels).' },
  { label: 'mouse', kind: 'var', doc: 'Pointer state: { x, y, down, inside }, in canvas points.' },
  { label: 'cache', kind: 'var', doc: 'Object that survives across frames and resets on recompile. Preloaded with cache.img.' },
  { label: 'Math', kind: 'var', doc: 'The standard JS Math object.' },
];

const KIND_LABEL = { fn: 'fn', enum: 'enum', var: 'var' };

// Build the flat entry list once: nvg members from the docs index, then every
// enum value. Enum names are exposed twice by the runtime (with and without the
// NVG_ prefix); only the short form is offered, but typing the long one still
// matches it.
export function buildEntries(docs, enums) {
  const entries = docs.map((d) => {
    const isFn = d.kind !== 'const';
    return {
      label: d.js,
      kind: isFn ? 'fn' : 'var',
      args: isFn ? d.args || [] : [],
      doc: d.doc || '',
      signature: d.signature || '',
      detail: isFn ? `(${(d.args || []).join(', ')})` : '',
      section: d.section || '',
      aliases: d.name ? [d.name] : [],
    };
  });

  for (const [name, value] of Object.entries(enums || {})) {
    if (name.startsWith('NVG_')) continue;
    entries.push({
      label: name,
      kind: 'enum',
      args: [],
      doc: '',
      signature: `NVG_${name} = ${value}`,
      detail: `= ${value}`,
      section: 'Enums',
      aliases: [`NVG_${name}`],
    });
  }
  return entries;
}

// Subsequence match, the behaviour every editor's quick-open trained people to
// expect: "rndrct" finds roundedRect. Returns a score (lower is better) or -1.
function score(label, query) {
  const l = label.toLowerCase();
  const q = query.toLowerCase();
  if (!q) return 1000; // no query: keep header order, see rank()
  if (l.startsWith(q)) return label.startsWith(query) ? 0 : 1;

  let li = 0;
  let gaps = 0;
  for (let qi = 0; qi < q.length; qi++) {
    const at = l.indexOf(q[qi], li);
    if (at === -1) return -1;
    gaps += at - li;
    li = at + 1;
  }
  return 100 + gaps + label.length * 0.01;
}

function rank(entries, query, limit) {
  const out = [];
  // With no query the only sensible order is the header's: nanovg.h groups calls
  // by topic, so `nvg.` opens on the composite/color/state functions in the same
  // order the docs list them, not alphabetically or shortest-first.
  if (!query) return entries.slice(0, limit);
  for (const e of entries) {
    let s = score(e.label, query);
    if (s < 0) {
      // Fall back to the C name so `nvgRoundedRect` finds `roundedRect`.
      for (const alias of e.aliases || []) {
        const as = score(alias, query);
        if (as >= 0) { s = as + 20; break; }
      }
    }
    if (s >= 0) out.push([s, e]);
  }
  out.sort((a, b) => a[0] - b[0] || a[1].label.length - b[1].label.length);
  return out.slice(0, limit).map(([, e]) => e);
}

const IDENT = /[A-Za-z0-9_$]/;

// What is being completed at the caret: a member of `nvg`, or a bare global.
function contextAt(value, caret) {
  let i = caret;
  while (i > 0 && IDENT.test(value[i - 1])) i--;
  const prefix = value.slice(i, caret);

  if (value[i - 1] === '.') {
    let j = i - 1;
    while (j > 0 && IDENT.test(value[j - 1])) j--;
    const object = value.slice(j, i - 1);
    if (object !== 'nvg') return null;
    return { kind: 'member', prefix, from: i, to: caret };
  }

  // Don't pop up inside a string or comment, or right after a digit.
  if (i > 0 && /[0-9]/.test(value[i - 1])) return null;
  return { kind: 'global', prefix, from: i, to: caret };
}

// Walk back to the innermost unclosed '(' and read the callee before it, so the
// signature strip can point at the argument the caret is actually in.
function callAt(value, caret) {
  let depth = 0;
  let arg = 0;
  for (let i = caret - 1; i >= 0; i--) {
    const c = value[i];
    if (c === ')' || c === ']' || c === '}') depth++;
    else if (c === '[' || c === '{') { if (depth === 0) return null; depth--; }
    else if (c === ',') { if (depth === 0) arg++; }
    else if (c === '\n' && depth === 0) {
      // A call left open across a newline is still a call; keep scanning.
      continue;
    } else if (c === '(') {
      if (depth > 0) { depth--; continue; }
      let j = i;
      while (j > 0 && IDENT.test(value[j - 1])) j--;
      const name = value.slice(j, i);
      if (!name) return null;
      const dotted = value[j - 1] === '.';
      let object = '';
      if (dotted) {
        let k = j - 1;
        while (k > 0 && IDENT.test(value[k - 1])) k--;
        object = value.slice(k, j - 1);
      }
      if (dotted && object !== 'nvg') return null;
      if (!dotted) return null;
      return { name, arg };
    }
  }
  return null;
}

const escapeHtml = (s) =>
  String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');

// Highlight the matched characters so it's obvious why a fuzzy hit is in the list.
function markMatch(label, query) {
  if (!query) return escapeHtml(label);
  let out = '';
  let li = 0;
  const q = query.toLowerCase();
  const l = label.toLowerCase();
  for (let qi = 0; qi < q.length; qi++) {
    const at = l.indexOf(q[qi], li);
    if (at === -1) return escapeHtml(label);
    out += escapeHtml(label.slice(li, at)) + `<b>${escapeHtml(label[at])}</b>`;
    li = at + 1;
  }
  return out + escapeHtml(label.slice(li));
}

const MAX_ITEMS = 40;

/** Popup + signature strip bound to one textarea.
 *
 * `metrics()` must return { x, y, lineHeight } for the caret, in the host's
 * coordinate space -- the editor owns that math since it owns the gutter and
 * scroll offsets.
 */
export function createCompleter({ input, host, metrics, onChange }) {
  const popup = document.createElement('div');
  popup.className = 'ac';
  popup.hidden = true;
  popup.innerHTML = '<div class="ac-list" role="listbox"></div><div class="ac-doc"></div>';
  host.appendChild(popup);

  const sig = document.createElement('div');
  sig.className = 'ac-sig';
  sig.hidden = true;
  host.appendChild(sig);

  const list = popup.querySelector('.ac-list');
  const docPane = popup.querySelector('.ac-doc');

  let entries = [];
  let byName = new Map();
  let items = [];
  let index = 0;
  let ctx = null;
  let manual = false; // opened with Ctrl+Space: show everything, keep it open

  const isOpen = () => !popup.hidden;

  function setEntries(next) {
    entries = next || [];
    byName = new Map(entries.filter((e) => e.kind === 'fn').map((e) => [e.label, e]));
  }

  function hide() {
    popup.hidden = true;
    items = [];
    ctx = null;
    manual = false;
  }

  function renderDoc() {
    const e = items[index];
    if (!e) { docPane.innerHTML = ''; return; }
    const head = e.kind === 'fn'
      ? `nvg.${escapeHtml(e.label)}(${escapeHtml(e.args.join(', '))})`
      : escapeHtml(e.label);
    // C signature before the prose: it's the shortest, most useful line and the
    // doc text is long enough to push it out of view otherwise.
    docPane.innerHTML =
      `<div class="ac-doc-head"><code>${head}</code></div>` +
      (e.signature ? `<code class="ac-doc-c">${escapeHtml(e.signature)}</code>` : '') +
      (e.doc ? `<p>${escapeHtml(e.doc)}</p>` : '');
    docPane.scrollTop = 0;
  }

  function renderList() {
    list.innerHTML = items
      .map((e, i) => `<div class="ac-item${i === index ? ' is-sel' : ''}" data-i="${i}" role="option">
        <span class="ac-kind ac-kind-${e.kind}">${KIND_LABEL[e.kind] || ''}</span>
        <span class="ac-label">${markMatch(e.label, ctx ? ctx.prefix : '')}</span>
        <span class="ac-detail">${escapeHtml(e.detail || '')}</span>
      </div>`)
      .join('');
    const sel = list.querySelector('.is-sel');
    if (sel) sel.scrollIntoView({ block: 'nearest' });
    renderDoc();
  }

  function place() {
    const { x, y, lineHeight } = metrics();
    const hostRect = host.getBoundingClientRect();
    popup.hidden = false; // must be laid out to measure
    const pw = popup.offsetWidth;
    const ph = popup.offsetHeight;
    const left = Math.max(4, Math.min(x, hostRect.width - pw - 4));
    // Flip above the caret when there isn't room below, like any IDE.
    const below = y + lineHeight;
    const top = below + ph > hostRect.height && y - ph > 0 ? y - ph : below;
    popup.style.left = `${left}px`;
    popup.style.top = `${top}px`;
  }

  function open(next) {
    items = next;
    index = 0;
    renderList();
    place();
  }

  function refresh({ force = false } = {}) {
    const caret = input.selectionStart;
    if (caret !== input.selectionEnd) { hide(); return; }

    const next = contextAt(input.value, caret);
    if (!next) { hide(); return; }

    // Unprompted popups while typing prose are noise; only volunteer after the
    // `nvg.` dot or two characters of an identifier.
    const wanted = force || manual
      || next.kind === 'member'
      || next.prefix.length >= 2;
    if (!wanted) { hide(); return; }

    const pool = next.kind === 'member' ? entries : SKETCH_GLOBALS;
    const found = rank(pool, next.prefix, MAX_ITEMS);
    if (!found.length) { hide(); return; }

    ctx = next;
    if (force) manual = true;
    open(found);
  }

  function accept(entry) {
    const e = entry || items[index];
    if (!e || !ctx) return false;
    const { from, to } = ctx;
    // Functions get their parens; zero-arg ones get closed so the caret can
    // move on, others leave the caret between them for the signature strip.
    const text = e.kind === 'fn' ? `${e.label}(${e.args.length ? '' : ')'}` : e.label;
    input.setRangeText(text, from, to, 'end');
    hide();
    onChange();
    updateSignature();
    return true;
  }

  function updateSignature() {
    if (isOpen()) { sig.hidden = true; return; }
    const call = callAt(input.value, input.selectionStart);
    const e = call && byName.get(call.name);
    if (!e || !e.args.length) { sig.hidden = true; return; }

    sig.innerHTML = `<code>nvg.${escapeHtml(e.label)}(${e.args
      .map((a, i) => (i === call.arg ? `<b>${escapeHtml(a)}</b>` : escapeHtml(a)))
      .join(', ')})</code>`;
    sig.hidden = false;
    const { x, y, lineHeight } = metrics();
    const hostRect = host.getBoundingClientRect();
    const sw = sig.offsetWidth;
    const sh = sig.offsetHeight;
    sig.style.left = `${Math.max(4, Math.min(x, hostRect.width - sw - 4))}px`;
    sig.style.top = `${y - sh - 2 >= 0 ? y - sh - 2 : y + lineHeight}px`;
  }

  list.addEventListener('mousedown', (ev) => {
    const row = ev.target.closest('.ac-item');
    if (!row) return;
    ev.preventDefault(); // don't blur the textarea
    accept(items[Number(row.dataset.i)]);
  });

  list.addEventListener('mousemove', (ev) => {
    const row = ev.target.closest('.ac-item');
    if (!row) return;
    const i = Number(row.dataset.i);
    if (i === index) return;
    index = i;
    renderList();
  });

  // Returns true when the key was consumed and the editor should not act on it.
  function handleKeyDown(ev) {
    const ctrlSpace = (ev.ctrlKey || ev.metaKey) && (ev.key === ' ' || ev.code === 'Space');
    if (ctrlSpace) {
      ev.preventDefault();
      refresh({ force: true });
      return true;
    }

    if (!isOpen()) return false;

    switch (ev.key) {
      case 'ArrowDown':
      case 'ArrowUp':
        ev.preventDefault();
        index = (index + (ev.key === 'ArrowDown' ? 1 : items.length - 1)) % items.length;
        renderList();
        return true;
      case 'PageDown':
      case 'PageUp':
        ev.preventDefault();
        index = ev.key === 'PageDown'
          ? Math.min(items.length - 1, index + 8)
          : Math.max(0, index - 8);
        renderList();
        return true;
      case 'Enter':
      case 'Tab':
        ev.preventDefault();
        accept();
        return true;
      case 'Escape':
        ev.preventDefault();
        hide();
        return true;
      case 'ArrowLeft':
      case 'ArrowRight':
      case 'Home':
      case 'End':
        hide();
        return false;
      default:
        return false;
    }
  }

  return {
    setEntries,
    handleKeyDown,
    refresh,
    updateSignature,
    isOpen,
    hide() { hide(); sig.hidden = true; },
  };
}
