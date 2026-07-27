// nanovg playground -- URL state.
//
// The hash is the document: a link reproduces a sketch exactly, which makes bug
// reports against nanovg pasteable.  Sketches compress well (deflate-raw via the
// native CompressionStream), with plain base64url as the fallback for browsers
// that lack the Compression Streams API.
//
//   #snippet=<id>   gallery entry
//   #code=<b64url>  deflate-raw payload
//   #raw=<b64url>   uncompressed payload

const PREFIX = { snippet: 'snippet=', code: 'code=', raw: 'raw=' };

const toBase64Url = (bytes) => {
  let s = '';
  for (const b of bytes) s += String.fromCharCode(b);
  return btoa(s).replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/, '');
};

const fromBase64Url = (text) => {
  const padded = text.replace(/-/g, '+').replace(/_/g, '/');
  const raw = atob(padded + '='.repeat((4 - (padded.length % 4)) % 4));
  return Uint8Array.from(raw, (c) => c.charCodeAt(0));
};

async function pipeThrough(bytes, transform) {
  const stream = new Blob([bytes]).stream().pipeThrough(transform);
  return new Uint8Array(await new Response(stream).arrayBuffer());
}

export async function encodeState(code) {
  const bytes = new TextEncoder().encode(code);
  if (typeof CompressionStream !== 'undefined') {
    try {
      const packed = await pipeThrough(bytes, new CompressionStream('deflate-raw'));
      return PREFIX.code + toBase64Url(packed);
    } catch {
      // fall through to the uncompressed form
    }
  }
  return PREFIX.raw + toBase64Url(bytes);
}

// Returns {snippet} | {code} | null.  Async because inflating a #code= payload
// requires the Compression Streams API.
export async function decodeState(hash) {
  const raw = (hash || '').replace(/^#/, '');
  if (!raw) return null;

  if (raw.startsWith(PREFIX.snippet)) {
    return { snippet: decodeURIComponent(raw.slice(PREFIX.snippet.length)) };
  }

  if (raw.startsWith(PREFIX.raw)) {
    try {
      return { code: new TextDecoder().decode(fromBase64Url(raw.slice(PREFIX.raw.length))) };
    } catch {
      return null;
    }
  }

  if (raw.startsWith(PREFIX.code)) {
    if (typeof DecompressionStream === 'undefined') return null;
    try {
      const bytes = fromBase64Url(raw.slice(PREFIX.code.length));
      const out = await pipeThrough(bytes, new DecompressionStream('deflate-raw'));
      return { code: new TextDecoder().decode(out) };
    } catch {
      return null;
    }
  }
  return null;
}
