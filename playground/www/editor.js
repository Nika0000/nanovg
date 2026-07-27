// nanovg playground -- code editor.
//
// A transparent <textarea> layered over a highlighted <pre>, plus a gutter.
// Deliberately dependency-free: CodeMirror/Monaco would need a bundler step and
// a CDN at runtime, and neither earns its keep for editing 40 lines of JS.
// Completion/signature help lives in complete.js and is driven from here,
// because caret geometry depends on this file's padding and scroll offsets.

import { createCompleter } from './complete.js';

const KEYWORDS = new Set([
  'const', 'let', 'var', 'function', 'return', 'if', 'else', 'for', 'while',
  'do', 'break', 'continue', 'new', 'typeof', 'instanceof', 'in', 'of', 'this',
  'true', 'false', 'null', 'undefined', 'switch', 'case', 'default', 'try',
  'catch', 'finally', 'throw', 'delete', 'void', 'await', 'async', 'class',
]);

const escapeHtml = (s) =>
  s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');

// Single-pass tokenizer: enough for JS sketches, and it degrades gracefully on
// anything it doesn't understand (falls through as plain text).
function highlight(src) {
  let out = '';
  let i = 0;
  const n = src.length;

  const push = (cls, text) =>
    (out += cls ? `<span class="tok-${cls}">${escapeHtml(text)}</span>` : escapeHtml(text));

  while (i < n) {
    const c = src[i];

    if (c === '/' && src[i + 1] === '/') {
      const end = src.indexOf('\n', i);
      const stop = end === -1 ? n : end;
      push('comment', src.slice(i, stop));
      i = stop;
      continue;
    }

    if (c === '/' && src[i + 1] === '*') {
      const end = src.indexOf('*/', i + 2);
      const stop = end === -1 ? n : end + 2;
      push('comment', src.slice(i, stop));
      i = stop;
      continue;
    }

    if (c === '"' || c === "'" || c === '`') {
      let j = i + 1;
      while (j < n && src[j] !== c) {
        if (src[j] === '\\') j++;
        if (src[j] === '\n' && c !== '`') break;
        j++;
      }
      push('string', src.slice(i, Math.min(j + 1, n)));
      i = j + 1;
      continue;
    }

    if (/[0-9]/.test(c) || (c === '.' && /[0-9]/.test(src[i + 1] || ''))) {
      let j = i;
      while (j < n && /[0-9a-fA-FxX._]/.test(src[j])) j++;
      push('number', src.slice(i, j));
      i = j;
      continue;
    }

    if (/[A-Za-z_$]/.test(c)) {
      let j = i;
      while (j < n && /[A-Za-z0-9_$]/.test(src[j])) j++;
      const word = src.slice(i, j);
      let cls = null;
      if (KEYWORDS.has(word)) cls = 'keyword';
      else if (word === 'nvg') cls = 'nvg';
      else if (src[j] === '(') cls = 'fn';
      else if (word === 'Math' || word === 'mouse' || word === 'cache') cls = 'builtin';
      push(cls, word);
      i = j;
      continue;
    }

    if (/[+\-*/%=<>!&|?:^~]/.test(c)) {
      let j = i;
      while (j < n && /[+\-*/%=<>!&|?:^~]/.test(src[j])) j++;
      push('op', src.slice(i, j));
      i = j;
      continue;
    }

    push(null, c);
    i++;
  }
  return out;
}

export function createEditor(root, { onChange, onRun }) {
  root.classList.add('editor');
  root.innerHTML = `
    <div class="editor-gutter" aria-hidden="true"></div>
    <div class="editor-scroll">
      <pre class="editor-highlight" aria-hidden="true"></pre>
      <textarea class="editor-input" spellcheck="false" autocomplete="off"
                autocapitalize="off" wrap="off"
                aria-label="Sketch source code"></textarea>
    </div>`;

  const gutter = root.querySelector('.editor-gutter');
  const scroll = root.querySelector('.editor-scroll');
  const pre = root.querySelector('.editor-highlight');
  const input = root.querySelector('.editor-input');

  let markers = [];

  // Caret geometry. The font is monospace, so one measured advance is enough --
  // no per-character mirror element. Measured lazily and cached, because at
  // create time the font may not have loaded yet and would give the fallback's
  // metrics.
  let charWidth = 0;
  const measureChar = () => {
    if (charWidth) return charWidth;
    const probe = document.createElement('span');
    probe.textContent = '0'.repeat(50);
    probe.style.cssText = 'position:absolute;visibility:hidden;white-space:pre';
    pre.appendChild(probe);
    charWidth = probe.getBoundingClientRect().width / 50;
    probe.remove();
    return charWidth;
  };

  const caretMetrics = () => {
    const upto = input.value.slice(0, input.selectionStart);
    const nl = upto.lastIndexOf('\n');
    const line = upto.length - upto.replace(/\n/g, '').length;
    const col = upto.length - (nl + 1);
    const pad = parseFloat(getComputedStyle(input).paddingLeft) || 0;
    const lineHeight = parseFloat(getComputedStyle(input).lineHeight) || 20;
    return {
      x: gutter.offsetWidth + pad + col * measureChar() - input.scrollLeft,
      y: pad + line * lineHeight - input.scrollTop,
      lineHeight,
    };
  };

  const completer = createCompleter({
    input,
    host: root,
    metrics: caretMetrics,
    onChange: () => {
      render();
      syncScroll();
      onChange(input.value);
    },
  });

  const renderGutter = () => {
    const lines = input.value.split('\n').length;
    const marked = new Map(markers.map((mk) => [mk.line, mk]));
    let html = '';
    for (let l = 1; l <= lines; l++) {
      const mk = marked.get(l);
      html += `<div class="gutter-line${mk ? ' has-error' : ''}"${
        mk ? ` title="${escapeHtml(mk.message)}"` : ''
      }>${mk ? '<span class="gutter-dot"></span>' : ''}${l}</div>`;
    }
    gutter.innerHTML = html;
  };

  const render = () => {
    // Trailing newline needs a placeholder or the <pre> collapses and the
    // highlight drifts out of sync with the textarea on the last line.
    pre.innerHTML = highlight(input.value) + '\n';
    renderGutter();
  };

  const syncScroll = () => {
    pre.style.transform = `translate(${-input.scrollLeft}px, ${-input.scrollTop}px)`;
    gutter.scrollTop = input.scrollTop;
  };

  input.addEventListener('input', () => {
    render();
    syncScroll();
    completer.refresh();
    completer.updateSignature();
    onChange(input.value);
  });
  input.addEventListener('scroll', () => {
    syncScroll();
    // Anchored to the caret, which just moved relative to the viewport.
    completer.hide();
  });
  input.addEventListener('blur', () => completer.hide());
  input.addEventListener('click', () => completer.updateSignature());
  // Arrow keys and clicks move the caret between arguments; keyup is the
  // cheapest place to notice that after the fact.
  input.addEventListener('keyup', (ev) => {
    if (ev.key.startsWith('Arrow') || ev.key === 'Home' || ev.key === 'End') {
      completer.updateSignature();
    }
  });

  input.addEventListener('keydown', (ev) => {
    // The popup owns navigation keys while it's open.
    if (completer.handleKeyDown(ev)) return;

    if ((ev.ctrlKey || ev.metaKey) && ev.key === 'Enter') {
      ev.preventDefault();
      onRun(input.value);
      return;
    }

    if (ev.key === 'Tab') {
      ev.preventDefault();
      const { selectionStart: a, selectionEnd: b, value } = input;
      if (a === b) {
        input.setRangeText('  ', a, b, 'end');
      } else {
        // Block indent / dedent on a multi-line selection.
        const start = value.lastIndexOf('\n', a - 1) + 1;
        const block = value.slice(start, b);
        const next = ev.shiftKey
          ? block.replace(/^ {1,2}/gm, '')
          : block.replace(/^/gm, '  ');
        input.setRangeText(next, start, b, 'preserve');
      }
      input.dispatchEvent(new Event('input'));
      return;
    }

    if (ev.key === 'Enter') {
      // Keep the current line's indentation, and add a level after an opener.
      const { selectionStart: a, value } = input;
      const lineStart = value.lastIndexOf('\n', a - 1) + 1;
      const line = value.slice(lineStart, a);
      const indent = (line.match(/^[ \t]*/) || [''])[0];
      const extra = /[{([]\s*$/.test(line) ? '  ' : '';
      ev.preventDefault();
      input.setRangeText('\n' + indent + extra, a, input.selectionEnd, 'end');
      input.dispatchEvent(new Event('input'));
    }
  });

  return {
    get value() {
      return input.value;
    },
    setValue(text) {
      input.value = text;
      completer.hide();
      render();
      syncScroll();
      input.scrollTop = 0;
    },
    /** Completion entries; see buildEntries() in complete.js. */
    setCompletions(entries) {
      completer.setEntries(entries);
    },
    focus() {
      input.focus();
    },
    setMarkers(next) {
      markers = next || [];
      renderGutter();
    },
    revealLine(line) {
      const el = gutter.children[line - 1];
      if (!el) return;
      const target = el.offsetTop - scroll.clientHeight / 2;
      input.scrollTop = Math.max(0, target);
      syncScroll();
    },
  };
}
