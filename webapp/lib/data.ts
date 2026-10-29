import { createClient } from "@/lib/supabase/server";
import { redirect } from "next/navigation";
import type { PetKind } from "@/lib/pets";

export type Pet = { kind: PetKind; name: string };
export type Day = { date: string; s25: number; s50: number; done_count: number; minutes: number; growth: number; streak: number };

// signed-in user + their pet (sends to onboarding if there is no pet yet)
export async function loadMe(opts: { needPet?: boolean } = { needPet: true }) {
  const supabase = await createClient();
  const { data: { user } } = await supabase.auth.getUser();
  if (!user) redirect("/");
  const { data: pet } = await supabase.from("pets").select("kind,name").eq("user_id", user.id).maybeSingle();
  if (opts.needPet && !pet) redirect("/onboarding");
  return { supabase, user, pet: pet as Pet | null };
}

export async function loadDays(supabase: Awaited<ReturnType<typeof createClient>>, userId: string) {
  const { data } = await supabase
    .from("day_sheets")
    .select("date,s25,s50,done_count,minutes,growth,streak")
    .eq("user_id", userId)
    .order("date", { ascending: false });
  return (data ?? []) as Day[];
}
