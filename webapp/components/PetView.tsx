import { PETS, type PetKind } from "@/lib/pets";

export default function PetView({ kind, stage, size = 240, hop = false }: { kind: PetKind; stage: number; size?: number; hop?: boolean }) {
  const look = PETS[kind].looks[stage];
  const px = Math.round(size * look.scale);
  return (
    <div className="petstage" style={{ height: size }}>
      <img
        src={`/pets/${look.img}`}
        alt={`${PETS[kind].label}, stage ${stage}`}
        width={px}
        height={px}
        className={hop ? "pet hop" : stage === 0 ? "pet wobble" : "pet breathe"}
      />
      <div className="shadow" style={{ width: px * 0.6 }} />
    </div>
  );
}
