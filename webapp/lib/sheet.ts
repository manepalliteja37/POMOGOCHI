// Pomodoro Rabbit day sheet (device sheet v1, from /api/day?d=YYYYMMDD) → parsed + validated + growth.
// Growth per day = 25-min focus ×1 + 50-min focus ×2 + done tasks ×1, max 8. Test (1/2/10 min) sessions don't count.

export const DAILY_CAP = 8;

export type ParsedSheet = {
  date: string; // YYYY-MM-DD
  s25: number;
  s50: number;
  doneCount: number;
  minutes: number;
  streak: number;
  growth: number;
  doneTasks: string[];
  deviceGrowth: number | null;
  petName: string | null;
  raw: unknown;
};

function int(v: unknown, max: number, field: string): number {
  if (typeof v !== "number" || !Number.isInteger(v) || v < 0 || v > max) {
    throw new Error(`"${field}" should be a whole number between 0 and ${max}`);
  }
  return v;
}

// today's date in India (the device runs on IST)
export function todayIST(): string {
  return new Date(Date.now() + 5.5 * 3600_000).toISOString().slice(0, 10);
}

function normDate(d: unknown): string {
  if (typeof d !== "string") throw new Error("missing \"date\"");
  let s = d.trim();
  if (/^\d{8}$/.test(s)) s = `${s.slice(0, 4)}-${s.slice(4, 6)}-${s.slice(6)}`;
  if (!/^\d{4}-\d{2}-\d{2}$/.test(s)) throw new Error(`"date" should look like 2026-10-25 (got ${d})`);
  const t = new Date(s + "T00:00:00Z");
  if (isNaN(t.getTime()) || t.toISOString().slice(0, 10) !== s) throw new Error(`"${s}" is not a real date`);
  if (s < "2024-01-01") throw new Error(`"${s}" is too old`);
  if (s > todayIST()) throw new Error(`"${s}" is in the future`);
  return s;
}

export function parseSheet(input: string | unknown): ParsedSheet {
  let j: any;
  if (typeof input === "string") {
    try {
      j = JSON.parse(input);
    } catch {
      throw new Error("this file isn't valid JSON");
    }
  } else j = input;

  if (!j || typeof j !== "object") throw new Error("this file isn't a day sheet");
  if (j.app !== "pomodoro") {
    throw new Error("not a Pomodoro Rabbit file (download it from the device web page → History → JSON)");
  }
  if (j.ver !== undefined && j.sheet === undefined) {
    throw new Error("this is a full backup (Export). Upload a day sheet from History → JSON instead");
  }
  if (j.sheet !== 1) throw new Error(`unknown sheet version ${j.sheet}`);

  const date = normDate(j.date);
  const f = j.focus ?? {};
  const s25 = int(f.s25 ?? 0, 60, "focus.s25");
  const s50 = int(f.s50 ?? 0, 30, "focus.s50");
  const minutes = int(f.minutes ?? 0, 1440, "focus.minutes");
  if (s25 * 25 + s50 * 50 > 1440) throw new Error("more focus than there are minutes in a day");

  const done = Array.isArray(j.done) ? j.done : [];
  if (done.length > 12) throw new Error("too many done tasks (max 12)");
  const doneTasks = done
    .map((t: any) => (typeof t?.task === "string" ? t.task.slice(0, 40) : ""))
    .filter(Boolean);

  const streak = typeof j.streak === "number" && j.streak >= 0 && j.streak < 10000 ? Math.floor(j.streak) : 0;
  const growth = Math.min(DAILY_CAP, s25 + 2 * s50 + done.length);

  return {
    date,
    s25,
    s50,
    doneCount: done.length,
    minutes,
    streak,
    growth,
    doneTasks,
    deviceGrowth: typeof j.pet?.growth === "number" ? j.pet.growth : null,
    petName: typeof j.pet?.name === "string" ? j.pet.name : null,
    raw: j,
  };
}
