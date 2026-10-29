"use server";

import { revalidatePath } from "next/cache";
import { redirect } from "next/navigation";
import { createClient } from "@/lib/supabase/server";
import { parseSheet } from "@/lib/sheet";
import { isKind } from "@/lib/pets";

async function needUser() {
  const supabase = await createClient();
  const { data: { user } } = await supabase.auth.getUser();
  if (!user) redirect("/");
  return { supabase, user };
}

// onboarding + settings: pick / switch pet, rename. Switching keeps all growth (growth lives in the day sheets).
export async function savePet(formData: FormData) {
  const { supabase, user } = await needUser();
  const kind = formData.get("kind");
  const name = String(formData.get("name") ?? "").trim().slice(0, 20);
  if (!isKind(kind) || !name) throw new Error("Pick a pet and give it a name");
  const { error } = await supabase
    .from("pets")
    .upsert({ user_id: user.id, kind, name, updated_at: new Date().toISOString() });
  if (error) throw new Error(error.message);
  revalidatePath("/home");
  redirect("/home");
}

export type UploadResult = { file: string; ok: boolean; date?: string; growth?: number; replaced?: boolean; error?: string };

export async function uploadSheets(files: { name: string; text: string }[]): Promise<UploadResult[]> {
  const { supabase, user } = await needUser();
  const out: UploadResult[] = [];
  for (const f of files.slice(0, 31)) {
    try {
      if (f.text.length > 20_000) throw new Error("file is too big for a day sheet");
      const s = parseSheet(f.text);
      const { data: had } = await supabase
        .from("day_sheets").select("date").eq("user_id", user.id).eq("date", s.date).maybeSingle();
      const { error } = await supabase.from("day_sheets").upsert({
        user_id: user.id,
        date: s.date,
        s25: s.s25,
        s50: s.s50,
        done_count: s.doneCount,
        minutes: s.minutes,
        growth: s.growth,
        streak: s.streak,
        raw: s.raw,
        uploaded_at: new Date().toISOString(),
      });
      if (error) throw new Error(error.message);
      out.push({ file: f.name, ok: true, date: s.date, growth: s.growth, replaced: !!had });
    } catch (e) {
      out.push({ file: f.name, ok: false, error: (e as Error).message });
    }
  }
  revalidatePath("/home");
  revalidatePath("/history");
  return out;
}

export async function deleteDay(formData: FormData) {
  const { supabase, user } = await needUser();
  const date = String(formData.get("date") ?? "");
  await supabase.from("day_sheets").delete().eq("user_id", user.id).eq("date", date);
  revalidatePath("/history");
  revalidatePath("/home");
}

export async function resetEverything() {
  const { supabase, user } = await needUser();
  await supabase.from("day_sheets").delete().eq("user_id", user.id);
  await supabase.from("pets").delete().eq("user_id", user.id);
  redirect("/onboarding");
}

export async function signOut() {
  const supabase = await createClient();
  await supabase.auth.signOut();
  redirect("/");
}
