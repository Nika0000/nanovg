'use client';

import { Play, Square, RotateCcw, ChevronDown, ChevronRight, Copy, Check } from 'lucide-react';
import { useEffect, useId, useRef, useState, useCallback } from 'react';
import { createHighlighterCoreSync } from 'shiki/core';
import { createJavaScriptRegexEngine } from 'shiki/engine/javascript';
import cpp from '@shikijs/langs/cpp';
import vitesseDark from '@shikijs/themes/vitesse-dark';
import vitesseLight from '@shikijs/themes/vitesse-light';

const highlighter = createHighlighterCoreSync({
  themes: [vitesseLight, vitesseDark],
  langs: [cpp],
  engine: createJavaScriptRegexEngine(),
});

function highlight(code: string): string {
  return highlighter.codeToHtml(code, {
    lang: 'cpp',
    themes: { light: 'vitesse-light', dark: 'vitesse-dark' },
  });
}

type Props = { code: string; title?: string; width?: number; height?: number; defaultCollapsed?: boolean };
const base = process.env.NEXT_PUBLIC_DOCS_BASE_PATH ?? '';

export function LiveExample({ code, title = 'main.cpp', width = 640, height = 240, defaultCollapsed = false }: Props) {
  const id = useId();
  const frame = useRef<HTMLIFrameElement>(null);
  const [value, setValue] = useState(code.trim());
  const [active, setActive] = useState(false);
  const [ready, setReady] = useState(false);
  const [status, setStatus] = useState('Run to load the C++ compiler and preview.');
  const [error, setError] = useState('');
  const [busy, setBusy] = useState(false);
  const [codeOpen, setCodeOpen] = useState(!defaultCollapsed);
  const [copied, setCopied] = useState(false);
  const pending = useRef<string | null>(null);
  const textareaRef = useRef<HTMLTextAreaElement>(null);
  const highlightRef = useRef<HTMLDivElement>(null);
  const storageKey = 'nanovg-example:' + title;

  const syncScroll = useCallback(() => {
    if (textareaRef.current && highlightRef.current) {
      highlightRef.current.scrollTop = textareaRef.current.scrollTop;
      highlightRef.current.scrollLeft = textareaRef.current.scrollLeft;
    }
  }, []);

  useEffect(() => {
    const saved = sessionStorage.getItem(storageKey);
    if (saved !== null) {
      setValue(saved);
      sessionStorage.removeItem(storageKey);
      if (crossOriginIsolated) {
        pending.current = saved;
        setActive(true);
        setBusy(true);
      }
    }
    const receive = (event: MessageEvent) => {
      if (event.source !== frame.current?.contentWindow || event.origin !== location.origin) return;
      if (event.data?.type === 'nanovg-ready') setReady(true);
      if (event.data?.type === 'nanovg-status') {
        setStatus(event.data.message);
        setBusy(Boolean(event.data.busy));
        setError(event.data.error || '');
      }
    };
    window.addEventListener('message', receive);
    return () => window.removeEventListener('message', receive);
  }, [storageKey]);

  useEffect(() => {
    if (ready && pending.current !== null) {
      frame.current?.contentWindow?.postMessage(
        { type: 'nanovg-run', code: pending.current, width, height }, location.origin,
      );
      pending.current = null;
    }
  }, [ready, width, height]);

  async function run() {
    setError('');
    setBusy(true);
    try {
      if (!crossOriginIsolated) {
        if (!isSecureContext || !('serviceWorker' in navigator)) {
          throw new Error('C++ previews require HTTPS (or localhost) and service worker support.');
        }
        if (sessionStorage.getItem('nanovg-isolation-reload')) {
          throw new Error('Browser isolation is unavailable. Enable service workers or serve the docs with COOP/COEP headers.');
        }
        setStatus('Preparing the compiler environment; this page will reload once…');
        sessionStorage.setItem(storageKey, value);
        await navigator.serviceWorker.register(base + '/preview-isolation.js', { scope: base + '/' });
        await Promise.race([
          navigator.serviceWorker.ready,
          new Promise((_, reject) => setTimeout(() => reject(new Error('Service worker setup timed out. Try again.')), 15000)),
        ]);
        sessionStorage.setItem('nanovg-isolation-reload', '1');
        location.reload();
        return;
      }
      if (!('OffscreenCanvas' in window)) throw new Error('This browser does not support OffscreenCanvas previews.');
      sessionStorage.removeItem('nanovg-isolation-reload');
      setStatus('Preparing C++ compiler…');
      if (ready) {
        frame.current?.contentWindow?.postMessage({ type: 'nanovg-run', code: value, width, height }, location.origin);
      } else {
        pending.current = value;
        setActive(true);
      }
    } catch (reason) {
      setError(reason instanceof Error ? reason.message : String(reason));
      setStatus('Preview unavailable');
      setBusy(false);
    }
  }

  function stop() {
    pending.current = null;
    frame.current?.contentWindow?.postMessage({ type: 'nanovg-stop' }, location.origin);
    setBusy(false);
    setStatus('Stopped');
  }

  function copyCode() {
    navigator.clipboard.writeText(value);
    setCopied(true);
    setTimeout(() => setCopied(false), 2000);
  }

  const showOverlay = busy || !!error;

  return (
    <section className="live-example not-prose" aria-label={title}>
      <div className="live-example-preview">
        {active ? (
          <iframe
            ref={frame}
            title={title + ' preview'}
            src={base + '/preview/index.html'}
            onLoad={() => frame.current?.contentWindow?.postMessage({ type: 'nanovg-connect' }, location.origin)}
            style={{ aspectRatio: width + ' / ' + height }}
          />
        ) : (
          <div className="live-example-placeholder" style={{ aspectRatio: width + ' / ' + height }}>
            <button type="button" onClick={run} disabled={busy}>Run example</button>
            <span>Compiles C++ in your browser. First run downloads the compiler.</span>
          </div>
        )}
        {showOverlay && (
          <div className="live-example-overlay" role="status" aria-live="polite">
            {error ? <span className="live-example-overlay-error">{error}</span> : <span>{status}</span>}
          </div>
        )}
      </div>
      <div className="live-example-code-header">
        <button type="button" onClick={() => setCodeOpen(!codeOpen)} className="live-example-collapse-btn">
          {codeOpen ? <ChevronDown size={14} /> : <ChevronRight size={14} />}
          <span>{title}</span>
        </button>
        <div className="live-example-actions">
          <button type="button" onClick={run} disabled={busy} title="Run">
            <Play size={14} />
          </button>
          <button type="button" onClick={stop} disabled={!active} title="Stop">
            <Square size={14} />
          </button>
          <button type="button" onClick={() => { stop(); setValue(code.trim()); }} title="Reset">
            <RotateCcw size={14} />
          </button>
          {codeOpen && (
            <button type="button" onClick={copyCode} title="Copy code">
              {copied ? <Check size={14} /> : <Copy size={14} />}
            </button>
          )}
        </div>
      </div>
      {codeOpen && (
        <div className="live-example-editor">
          <div
            ref={highlightRef}
            className="live-example-highlight"
            aria-hidden="true"
            dangerouslySetInnerHTML={{ __html: highlight(value) }}
          />
          <textarea
            ref={textareaRef}
            id={id + '-code'}
            aria-labelledby={id}
            value={value}
            onChange={(event) => setValue(event.target.value)}
            onScroll={syncScroll}
            onKeyDown={(event) => {
              if ((event.ctrlKey || event.metaKey) && event.key === 'Enter' && !busy) {
                event.preventDefault();
                void run();
              }
            }}
            spellCheck={false}
            autoCapitalize="off"
            autoCorrect="off"
            rows={Math.min(22, Math.max(6, value.split('\n').length))}
          />
        </div>
      )}
    </section>
  );
}
