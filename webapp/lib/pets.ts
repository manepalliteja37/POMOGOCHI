// Pet kinds, stages and art (Microsoft Fluent Emoji 3D, MIT, in /public/pets).

export type PetKind = "dog" | "rabbit" | "parrot" | "plant";

export const STAGES = [
  { name: "Egg", at: 0 },
  { name: "Baby", at: 1 },
  { name: "Kid", at: 10 },
  { name: "Teen", at: 40 },
  { name: "Adult", at: 100 },
] as const;

type Look = { img: string; scale: number };

// index = stage (0 Egg … 4 Adult)
export const PETS: Record<PetKind, { label: string; emoji: string; looks: Look[]; zeroText: string }> = {
  dog: {
    label: "Dog",
    emoji: "🐶",
    zeroText: "A puppy is on the way",
    looks: [
      { img: "egg_3d.png", scale: 0.6 },
      { img: "dog_face_3d.png", scale: 0.7 },
      { img: "dog_3d.png", scale: 0.75 },
      { img: "dog_3d.png", scale: 0.88 },
      { img: "dog_3d.png", scale: 1 },
    ],
  },
  rabbit: {
    label: "Rabbit",
    emoji: "🐰",
    zeroText: "A bunny is on the way",
    looks: [
      { img: "egg_3d.png", scale: 0.6 },
      { img: "rabbit_face_3d.png", scale: 0.7 },
      { img: "rabbit_3d.png", scale: 0.75 },
      { img: "rabbit_3d.png", scale: 0.88 },
      { img: "rabbit_3d.png", scale: 1 },
    ],
  },
  parrot: {
    label: "Parrot",
    emoji: "🦜",
    zeroText: "An egg, waiting to hatch",
    looks: [
      { img: "egg_3d.png", scale: 0.6 },
      { img: "egg_3d.png", scale: 0.75 },
      { img: "hatching_chick_3d.png", scale: 0.8 },
      { img: "parrot_3d.png", scale: 0.88 },
      { img: "parrot_3d.png", scale: 1 },
    ],
  },
  plant: {
    label: "Plant",
    emoji: "🌱",
    zeroText: "A seed in the soil",
    looks: [
      { img: "seedling_3d.png", scale: 0.45 },
      { img: "seedling_3d.png", scale: 0.7 },
      { img: "herb_3d.png", scale: 0.8 },
      { img: "potted_plant_3d.png", scale: 0.9 },
      { img: "sunflower_3d.png", scale: 1 },
    ],
  },
};

export const KINDS = Object.keys(PETS) as PetKind[];

export function isKind(k: unknown): k is PetKind {
  return typeof k === "string" && (KINDS as string[]).includes(k);
}

export function stageOf(growth: number) {
  let i = 0;
  for (let s = 0; s < STAGES.length; s++) if (growth >= STAGES[s].at) i = s;
  const next = STAGES[i + 1];
  const from = STAGES[i].at;
  const pct = next ? Math.round(((growth - from) / (next.at - from)) * 100) : 100;
  return { index: i, name: STAGES[i].name, next: next?.name ?? null, toNext: next ? next.at - growth : 0, pct };
}
