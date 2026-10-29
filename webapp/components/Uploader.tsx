"use client";
import { useState } from "react";
import { useRouter } from "next/navigation";
import { parseSheet, type ParsedSheet } from "@/lib/sheet";
import { uploadSheets, type UploadResult } from "@/app/actions";

type Item = { name: string; text: string; sheet?: ParsedSheet; error?: string };

export default function Uploader() {
  const [items, setItems] = useState<Item[]>([]);
  const [drag, setDrag] = useState(false);
  const [busy, setBusy] = useState(false);
  const [results, setResults] = useState<UploadResult[] | null>(null);
  const router = useRouter();

  async function addFiles(list: FileList | null) {
    if (!list) return;
    setResults(null);
    const next: Item[] = [];
    for (const f of Array.from(list).slice(0, 31)) {
      const text = await f.text();
      try {
        next.push({ name: f.name, text, sheet: parseSheet(text) });
      } catch (e) {
        next.push({ name: f.name, text, error: (e as Error).message });
      }
    }
    // one entry per date: a later file for the same day wins
    const byKey = new Map<string, Item>();
    for (const it of [...items, ...next]) byKey.set(it.sheet?.date ?? it.name, it);
    setItems([...byKey.values()].sort((a, b) => (b.sheet?.date ?? "").localeCompare(a.sheet?.date ?? "")));
  }

  const good = items.filter((i) => i.sheet);
  const totalGrowth = good.reduce((a, i) => a + (i.sheet?.growth ?? 0), 0);

  async function confirm() {
    setBusy(true);
    const res = await uploadSheets(good.map((i) => ({ name: i.name, text: i.text })));
    setBusy(false);
    setResults(res);
    if (res.every((r) => r.ok)) router.push("/home?hop=1");
  }

  return (
    <div>
      <label
        className={drag ? "drop on" : "drop"}
        onDragOver={(e) => { e.preventDefault(); setDrag(true); }}
        onDragLeave={() => setDrag(false)}
        onDrop={(e) => { e.preventDefault(); setDrag(false); addFiles(e.dataTransfer.files); }}
      >
        <input type="file" accept=".json,application/json" multiple onChange={(e) => addFiles(e.target.files)} />
        <span className="big">📄</span>
        <span>Drop .json files here, or tap to pick</span>
      </label>

      {items.length > 0 && (
        <div className="card list">
          {items.map((it) => (
            <div key={it.name + (it.sheet?.date ?? "")} className={it.error ? "rowi bad" : "rowi"}>
              {it.sheet ? (
                <>
                  <b>{it.sheet.date}</b>
                  <span>🍅25×{it.sheet.s25} · 🍅50×{it.sheet.s50} · ✅{it.sheet.doneCount} · {it.sheet.minutes} min</span>
                  <em>+{it.sheet.growth}</em>
                </>
              ) : (
                <>
                  <b>{it.name}</b>
                  <span>{it.error}</span>
                  <em>✕</em>
                </>
              )}
            </div>
          ))}
          <div className="rowi sum">
            <b>{good.length} day{good.length === 1 ? "" : "s"}</b>
            <span>max 8 growth a day · test sessions don&apos;t count</span>
            <em>+{totalGrowth}</em>
          </div>
        </div>
      )}

      {good.length > 0 && (
        <div className="actions">
          <button className="btn ghost" onClick={() => { setItems([]); setResults(null); }} disabled={busy}>Clear</button>
          <button className="btn primary big" onClick={confirm} disabled={busy}>
            {busy ? "Uploading…" : `Confirm +${totalGrowth} growth`}
          </button>
        </div>
      )}

      {results && results.some((r) => !r.ok) && (
        <div className="card list">
          {results.map((r) => (
            <div key={r.file} className={r.ok ? "rowi" : "rowi bad"}>
              <b>{r.date ?? r.file}</b>
              <span>{r.ok ? (r.replaced ? "replaced" : "saved") : r.error}</span>
              <em>{r.ok ? `+${r.growth}` : "✕"}</em>
            </div>
          ))}
        </div>
      )}
    </div>
  );
}
