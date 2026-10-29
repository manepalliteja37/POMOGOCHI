import Link from "next/link";
import Nav from "@/components/Nav";
import PetView from "@/components/PetView";
import { loadMe, loadDays } from "@/lib/data";
import { PETS, stageOf } from "@/lib/pets";
import { todayIST } from "@/lib/sheet";

export default async function Home({ searchParams }: { searchParams: Promise<{ hop?: string }> }) {
  const { supabase, user, pet } = await loadMe();
  const days = await loadDays(supabase, user.id);
  const { hop } = await searchParams;

  const total = days.reduce((a, d) => a + d.growth, 0);
  const st = stageOf(total);
  const latest = days[0];
  const today = days.find((d) => d.date === todayIST());
  const week = days.filter((d) => d.date > new Date(Date.now() + 5.5 * 3600_000 - 7 * 86400_000).toISOString().slice(0, 10));
  const weekMin = week.reduce((a, d) => a + d.minutes, 0);
  const p = PETS[pet!.kind];

  return (
    <>
      <Nav active="home" />
      <main className="page">
        <section className="card hero">
          <PetView kind={pet!.kind} stage={st.index} hop={!!hop} />
          <h1>{pet!.name}</h1>
          <p className="stage">
            {p.emoji} {st.name} {p.label}
          </p>
          {st.index === 0 && <p className="hint">{p.zeroText}. Upload your first day to hatch it!</p>}
          <div className="bar">
            <div style={{ width: `${st.pct}%` }} />
          </div>
          <p className="hint">
            {st.next ? `${total} growth · ${st.toNext} more to ${st.next}` : `${total} growth · fully grown 🎉`}
          </p>
        </section>

        <section className="stats">
          <div className="stat"><b>🔥 {latest?.streak ?? 0}</b><span>day streak</span></div>
          <div className="stat"><b>🍅 {today ? today.s25 + today.s50 : 0}</b><span>focus today</span></div>
          <div className="stat"><b>✅ {today?.done_count ?? 0}</b><span>tasks today</span></div>
          <div className="stat"><b>⏱️ {Math.round(weekMin / 6) / 10}h</b><span>last 7 days</span></div>
        </section>

        {!today && (
          <Link href="/upload" className="btn primary big block">
            📤 Upload today&apos;s sheet
          </Link>
        )}
        {latest && <p className="hint center">Last upload: {latest.date}</p>}
      </main>
    </>
  );
}
