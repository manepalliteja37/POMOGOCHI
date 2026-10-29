"use client";
import { useState } from "react";
import { PETS, KINDS, type PetKind } from "@/lib/pets";
import { savePet } from "@/app/actions";

export default function PetPicker({ initialKind, initialName }: { initialKind: PetKind | null; initialName: string }) {
  const [kind, setKind] = useState<PetKind | null>(initialKind);
  const [name, setName] = useState(initialName);
  const [busy, setBusy] = useState(false);
  return (
    <form action={savePet} onSubmit={() => setBusy(true)} className="picker">
      <div className="choices">
        {KINDS.map((k) => (
          <label key={k} className={kind === k ? "choice on" : "choice"}>
            <input type="radio" name="kind" value={k} checked={kind === k} onChange={() => setKind(k)} />
            <img src={`/pets/${PETS[k].looks[4].img}`} alt={PETS[k].label} className={kind === k ? "pet hop" : "pet"} />
            <span>{PETS[k].label}</span>
          </label>
        ))}
      </div>
      <input
        className="name"
        name="name"
        maxLength={20}
        placeholder="Give it a name"
        value={name}
        onChange={(e) => setName(e.target.value)}
      />
      <button className="btn primary big" disabled={!kind || !name.trim() || busy}>
        {busy ? "Saving…" : initialKind ? "Save" : "Let's go"}
      </button>
      {initialKind && <p className="hint">Switching pets keeps all your growth.</p>}
    </form>
  );
}
