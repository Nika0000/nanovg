// nanovg playground -- snippet gallery.
//
// One concept per snippet, each short enough to read in full on screen.  These
// are teaching material, not a port of examples/demo.c: the native demo is a
// stress test, these are meant to isolate a single idea.
//
// Every snippet body runs once per frame with (nvg, t, w, h, mouse, cache) in
// scope.  See www/README or the ? panel for the contract.

export const SNIPPETS = [
  {
    id: 'hello',
    title: 'Hello, nanovg',
    blurb: 'Path, fill, stroke — the whole loop in six calls.',
    code: `// A path is built, then painted. Fill and stroke read the
// styles that were current when you called them.
nvg.beginPath();
nvg.roundedRect(w / 2 - 140, h / 2 - 70, 280, 140, 16);

nvg.fillColor(nvg.hex('#2b3a5b'));
nvg.fill();

nvg.lineStyle(nvg.LINE_DASHED);
nvg.strokeWidth(2);
nvg.strokeColor(nvg.hex('#7aa2f7'));
nvg.stroke();

nvg.fontFace('sans');
nvg.fontSize(22);
nvg.fillColor(nvg.hex('#c0caf5'));
nvg.textAlign(nvg.ALIGN_CENTER | nvg.ALIGN_MIDDLE);
nvg.text(w / 2, h / 2, 'Hello, nanovg');
`,
  },
  {
    id: 'winding',
    title: 'Winding & holes',
    blurb: 'Sub-path direction decides solid vs. hole under the even-odd rule.',
    code: `// Shapes are CCW by default. Flipping a sub-path to CW turns it
// into a hole in the sub-path that encloses it.
nvg.fillColor(nvg.hex('#7aa2f7'));

// Left: two CCW circles -> both solid, overlap is still solid.
nvg.beginPath();
nvg.circle(w * 0.3, h / 2, 80);
nvg.circle(w * 0.3, h / 2, 40);
nvg.fill();

// Right: the inner circle is CW -> punched out.
nvg.beginPath();
nvg.circle(w * 0.7, h / 2, 80);
nvg.circle(w * 0.7, h / 2, 40);
nvg.pathWinding(nvg.HOLE);
nvg.fill();

nvg.fontFace('sans');
nvg.fontSize(14);
nvg.textAlign(nvg.ALIGN_CENTER);
nvg.fillColor(nvg.hex('#565f89'));
nvg.text(w * 0.3, h / 2 + 120, 'both CCW');
nvg.text(w * 0.7, h / 2 + 120, 'inner pathWinding(HOLE)');
`,
  },
  {
    id: 'joins',
    title: 'Joins & caps',
    blurb: 'lineJoin, lineCap and miterLimit on the same zig-zag.',
    code: `const joins = [nvg.MITER, nvg.ROUND, nvg.BEVEL];
const caps  = [nvg.BUTT, nvg.ROUND, nvg.SQUARE];
const names = ['MITER / BUTT', 'ROUND / ROUND', 'BEVEL / SQUARE'];

nvg.fontFace('sans');
nvg.fontSize(13);
nvg.textAlign(nvg.ALIGN_CENTER);

for (let i = 0; i < 3; i++) {
  const y = 70 + i * (h - 100) / 3;

  nvg.lineJoin(joins[i]);
  nvg.lineCap(caps[i]);
  nvg.strokeWidth(18);
  nvg.miterLimit(10);
  nvg.strokeColor(nvg.hex('#7dcfff'));

  nvg.beginPath();
  nvg.moveTo(w * 0.25, y + 30);
  nvg.lineTo(w * 0.42, y - 20);
  nvg.lineTo(w * 0.58, y + 30);
  nvg.lineTo(w * 0.75, y - 20);
  nvg.stroke();

  nvg.fillColor(nvg.hex('#565f89'));
  nvg.text(w * 0.5, y + 52, names[i]);
}
`,
  },
  {
    id: 'gradients',
    title: 'Gradients',
    blurb: 'linear, radial and box paints — and why box gradients make shadows.',
    code: `const cardW = Math.min(200, (w - 100) / 3);
const cardH = 150;
const y = h / 2 - cardH / 2;
const gap = (w - cardW * 3) / 4;
const x = (i) => gap + i * (cardW + gap);

const a = nvg.hex('#7aa2f7');
const b = nvg.hex('#bb9af7');

// Paints are values: build one, hand it to fillPaint, draw.
nvg.beginPath();
nvg.roundedRect(x(0), y, cardW, cardH, 8);
nvg.fillPaint(nvg.linearGradient(x(0), y, x(0), y + cardH, a, b));
nvg.fill();

nvg.beginPath();
nvg.roundedRect(x(1), y, cardW, cardH, 8);
nvg.fillPaint(nvg.radialGradient(x(1) + cardW / 2, y + cardH / 2, 10, cardH * 0.6, a, b));
nvg.fill();

// A box gradient is a feathered rounded rect: the shadow primitive.
nvg.beginPath();
nvg.rect(x(2) - 20, y - 20, cardW + 40, cardH + 40);
nvg.roundedRect(x(2), y, cardW, cardH, 8);
nvg.pathWinding(nvg.HOLE);
nvg.fillPaint(nvg.boxGradient(x(2), y + 4, cardW, cardH, 8, 16,
                              nvg.rgba(0, 0, 0, 160), nvg.rgba(0, 0, 0, 0)));
nvg.fill();

nvg.beginPath();
nvg.roundedRect(x(2), y, cardW, cardH, 8);
nvg.fillColor(nvg.hex('#24283b'));
nvg.fill();

nvg.fontFace('sans');
nvg.fontSize(13);
nvg.textAlign(nvg.ALIGN_CENTER);
nvg.fillColor(nvg.hex('#565f89'));
['linearGradient', 'radialGradient', 'boxGradient as shadow'].forEach((label, i) => {
  nvg.text(x(i) + cardW / 2, y + cardH + 34, label);
});
`,
  },
  {
    id: 'transforms',
    title: 'Transform stack',
    blurb: 'save/restore around translate, rotate and scale.',
    code: `// Transforms pre-multiply the current matrix, so nesting composes.
// save()/restore() is the only way back.
const petals = 12;

nvg.save();
nvg.translate(w / 2, h / 2);
nvg.rotate(t * 0.3);

for (let i = 0; i < petals; i++) {
  nvg.save();
  nvg.rotate((i / petals) * Math.PI * 2);
  nvg.translate(90, 0);
  nvg.scale(1, 0.4 + 0.3 * Math.sin(t * 2 + i));

  nvg.beginPath();
  nvg.circle(0, 0, 34);
  nvg.fillColor(nvg.hsla(i / petals, 0.6, 0.62, 200));
  nvg.fill();
  nvg.restore();
}
nvg.restore();

nvg.beginPath();
nvg.circle(w / 2, h / 2, 26);
nvg.fillColor(nvg.hex('#1a1b26'));
nvg.fill();
`,
  },
  {
    id: 'scissor',
    title: 'Scissor clipping',
    blurb: 'scissor, intersectScissor and how the transform applies to both.',
    code: `// The scissor rect is transformed like everything else, and
// intersectScissor() ands it with what is already active.
const boxW = 220, boxH = 160;
const bx = w / 2 - boxW / 2, by = h / 2 - boxH / 2;

nvg.save();
nvg.scissor(bx, by, boxW, boxH);

// A rotated second rect narrows the region further.
nvg.save();
nvg.translate(w / 2, h / 2);
nvg.rotate(Math.sin(t) * 0.5);
nvg.intersectScissor(-90, -110, 180, 220);
nvg.restore();

// Stripes, clipped to the intersection.
for (let i = -20; i < 40; i++) {
  nvg.beginPath();
  nvg.rect(bx + i * 16 + (t * 30) % 32, by - 40, 8, boxH + 80);
  nvg.fillColor(nvg.hsla(0.58 + (i % 5) * 0.02, 0.7, 0.6, 255));
  nvg.fill();
}
nvg.restore();

nvg.beginPath();
nvg.rect(bx, by, boxW, boxH);
nvg.strokeWidth(1);
nvg.strokeColor(nvg.hex('#565f89'));
nvg.stroke();
`,
  },
  {
    id: 'text',
    title: 'Text & metrics',
    blurb: 'align flags, textBounds, textMetrics, and wrapped text boxes.',
    code: `const msg = 'Measure, then draw.';
const x = 60, y = 110;

nvg.fontFace('sans');
nvg.fontSize(34);
nvg.textAlign(nvg.ALIGN_LEFT | nvg.ALIGN_BASELINE);

// textBounds() reports local space, unaffected by the current transform.
const tb = nvg.textBounds(x, y, msg);
nvg.beginPath();
nvg.rect(tb.xmin, tb.ymin, tb.width, tb.height);
nvg.fillColor(nvg.rgba(122, 162, 247, 40));
nvg.fill();

// Baseline, ascender and descender from textMetrics().
const met = nvg.textMetrics();
nvg.strokeWidth(1);
[[y, '#7aa2f7'], [y + met.ascender, '#9ece6a'], [y + met.descender, '#f7768e']]
  .forEach(([ly, color]) => {
    nvg.beginPath();
    nvg.moveTo(x - 20, ly);
    nvg.lineTo(x + tb.width + 20, ly);
    nvg.strokeColor(nvg.hex(color));
    nvg.stroke();
  });

nvg.fillColor(nvg.hex('#c0caf5'));
nvg.text(x, y, msg);

// textBox() wraps at word boundaries.
nvg.fontFace('sans');
nvg.fontSize(16);
nvg.fillColor(nvg.hex('#a9b1d6'));
nvg.textLineHeight(1.4);
nvg.textBox(x, y + 70, w - x * 2,
  'textBox wraps at the given width, splitting on word boundaries and ' +
  'honouring newlines. Words wider than the row are broken at the ' +
  'nearest character, since nanovg does not hyphenate.');
`,
  },
  {
    id: 'composite',
    title: 'Composite operations',
    blurb: 'globalCompositeOperation, modelled on the HTML canvas ops.',
    code: `const ops = [
  [nvg.SOURCE_OVER, 'SOURCE_OVER'],
  [nvg.SOURCE_IN, 'SOURCE_IN'],
  [nvg.SOURCE_OUT, 'SOURCE_OUT'],
  [nvg.ATOP, 'ATOP'],
  [nvg.LIGHTER, 'LIGHTER'],
  [nvg.XOR, 'XOR'],
];

nvg.fontFace('sans');
nvg.fontSize(12);
nvg.textAlign(nvg.ALIGN_CENTER);

const cols = 3;
const cellW = w / cols, cellH = h / 2;

ops.forEach(([op, name], i) => {
  const cx = (i % cols) * cellW + cellW / 2;
  const cy = Math.floor(i / cols) * cellH + cellH / 2 - 10;

  nvg.save();
  nvg.scissor(cx - cellW / 2 + 4, cy - cellH / 2 + 4, cellW - 8, cellH - 8);

  // Destination.
  nvg.globalCompositeOperation(nvg.SOURCE_OVER);
  nvg.beginPath();
  nvg.circle(cx - 18, cy, 40);
  nvg.fillColor(nvg.rgba(122, 162, 247, 255));
  nvg.fill();

  // Source, blended with the op under test.
  nvg.globalCompositeOperation(op);
  nvg.beginPath();
  nvg.circle(cx + 18, cy, 40);
  nvg.fillColor(nvg.rgba(247, 118, 142, 255));
  nvg.fill();

  nvg.globalCompositeOperation(nvg.SOURCE_OVER);
  nvg.restore();

  nvg.fillColor(nvg.hex('#565f89'));
  nvg.text(cx, cy + 62, name);
});
`,
  },
  {
    id: 'images',
    title: 'Image patterns',
    blurb: 'imagePattern as a fill paint, clipped by an arbitrary path.',
    code: `// Images are paints, so any path can be an image mask.
const img = cache.img;
const size = nvg.imageSize(img);
const scale = Math.min(w / size.width, h / size.height) * 0.8;
const iw = size.width * scale, ih = size.height * scale;
const ix = w / 2 - iw / 2, iy = h / 2 - ih / 2;

const paint = nvg.imagePattern(ix, iy, iw, ih, 0, img, 1.0);

nvg.beginPath();
const r = 60 + Math.sin(t * 1.5) * 40;
nvg.roundedRect(ix, iy, iw, ih, r);
nvg.fillPaint(paint);
nvg.fill();

nvg.strokeWidth(2);
nvg.strokeColor(nvg.rgba(255, 255, 255, 60));
nvg.stroke();
`,
    needs: ['image'],
  },
  {
    id: 'cost',
    title: 'What a frame costs',
    blurb: 'Read the inspector: one call per paint op, but not all calls are equal.',
    code: `// nanovg queues one draw call per fill(), stroke() and text() -- it does
// not merge them, so the paint-op count IS the call count. What varies is
// what each call has to do, and the breakdown in the inspector names it:
//
//   convex fill        one pass, no stencil            cheapest
//   fill (stencil+cover)  two passes over the shape    concave paths
//   stroke             expanded geometry
//   triangles          text, from the glyph atlas
//
// So the lever is vertices per call, not calls per shape. One text() draws
// a whole string in a single call however long it is; splitting the same
// string into ten calls costs ten. Edit COUNT and SPLIT_TEXT and watch.

const COUNT = 12;
const SPLIT_TEXT = false;

// COUNT circles -> COUNT "convex fill" calls.
nvg.fillColor(nvg.hex('#7aa2f7'));
for (let i = 0; i < COUNT; i++) {
  nvg.beginPath();
  nvg.circle(60 + (i % 4) * 44, 90 + Math.floor(i / 4) * 44, 17);
  nvg.fill();
}

// A ring is concave -> still one call, but stencil-then-cover.
nvg.beginPath();
nvg.circle(w - 100, 130, 54);
nvg.circle(w - 100, 130, 27);
nvg.pathWinding(nvg.HOLE);
nvg.fillColor(nvg.hex('#bb9af7'));
nvg.fill();

nvg.beginPath();
nvg.moveTo(60, h - 80);
nvg.bezierTo(w * 0.35, h - 180, w * 0.65, h - 10, w - 60, h - 100);
nvg.strokeWidth(6);
nvg.strokeColor(nvg.hex('#7dcfff'));
nvg.stroke();

// One string, one call -- or one call per word, if you ask for that.
const line = 'one text call draws this entire string';
nvg.fontFace('sans');
nvg.fontSize(17);
nvg.fillColor(nvg.hex('#a9b1d6'));
nvg.textAlign(nvg.ALIGN_LEFT);

if (SPLIT_TEXT) {
  let x = 60;
  for (const word of line.split(' ')) {
    x += nvg.text(x, h * 0.55, word + ' ') - x;
  }
} else {
  nvg.text(60, h * 0.55, line);
}

nvg.fontSize(12);
nvg.fillColor(nvg.hex('#565f89'));
nvg.text(60, 62, COUNT + ' convex fills');
nvg.text(w - 165, 62, '1 concave fill (ring)');
nvg.text(60, h * 0.55 + 26, SPLIT_TEXT ? 'split: one call per word' : 'whole string: 1 call');
`,
  },
  {
    id: 'interactive',
    title: 'Mouse & state',
    blurb: 'The mouse object, plus cache for anything that must survive a frame.',
    code: `// cache persists across frames (but resets on recompile), so it is
// where trails, physics and lazily built data belong.
cache.trail = cache.trail || [];
cache.trail.push({ x: mouse.x, y: mouse.y, born: t });
if (cache.trail.length > 90) cache.trail.shift();

nvg.fontFace('sans');
nvg.fontSize(14);
nvg.fillColor(nvg.hex('#565f89'));
nvg.textAlign(nvg.ALIGN_CENTER);
nvg.text(w / 2, 40, mouse.inside ? 'move the pointer' : 'pointer is outside the canvas');

cache.trail.forEach((p, i) => {
  const age = t - p.born;
  const alpha = Math.max(0, 1 - age * 1.2);
  if (alpha <= 0) return;
  nvg.beginPath();
  nvg.circle(p.x, p.y, 4 + (i / cache.trail.length) * 18);
  nvg.fillColor(nvg.hsla(0.6 + age * 0.1, 0.7, 0.65, alpha * 140));
  nvg.fill();
});

if (mouse.down) {
  nvg.beginPath();
  nvg.circle(mouse.x, mouse.y, 40);
  nvg.strokeWidth(3);
  nvg.strokeColor(nvg.hex('#e0af68'));
  nvg.stroke();
}
`,
  },
];

export const DEFAULT_SNIPPET = 'hello';

export function findSnippet(id) {
  return SNIPPETS.find((s) => s.id === id);
}
