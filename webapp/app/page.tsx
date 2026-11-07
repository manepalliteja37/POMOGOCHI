import Nav from "@/components/Nav";
import { loadMe, loadDays } from "@/lib/data";
import { deleteDay } from "@/app/actions";
import { todayIST } from "@/lib/sheet";

function shift(d: string, n: number) {
  const t = new Date(d + "T00:00:00Z");
  t.setUTCDate(t.getUTCDate() + n);
  return t.toISOString().slice(0, 10);
}

export default async function History() {
  const { supabase, user } = await loadMe();
  const days = await loadDays(supabase, user.id);
  const by = new Map(days.map((d) => [d.date, d]));
  const today = todayIST();

  // last 5 weeks, Sunday-first, ending this week
  const dow = new Date(today + "T00:00:00Z").getUTCDay();
  const start = shift(today, -dow - 28);
  const cells = Array.from({ length: 35 }, (_, i) => shift(start, i));

  return (
    <>
      <Nav active="history" />
      <main className="page">
        <h1>History</h1>
        <div className="card">
          <div className="cal">
            {["S", "M", "T", "W", "T", "F", "S"].map((d, i) => (
              <div key={i} className="dh">{d}</div>
            ))}
            {cells.map((d) => {
              const g = by.get(d)?.growth ?? 0;
              const lvl = d > today ? "future" : g === 0 ? (by.has(d) ? "l0 up" : "l0") : g < 3 ? "l1" : g < 6 ? "l2" : "l3";
              return (
                <div key={d} className={`dc ${lvl}`} title={by.has(d) ? `${d}: +${g}` : d}>
                  {Number(d.slice(8))}
                </div>
              );
            })}
          </div>
          <p className="hint center">Darker = more growth that day</p>
        </div>

        <div className="card list">
          {days.length === 0 && <p className="hint center">No days uploaded yet.</p>}
          {days.slice(0, 60).map((d) => (
            <div key={d.date} className="rowi">
              <b>{d.date}</b>
              <span>🍅25×{d.s25} · 🍅50×{d.s50} · ✅{d.done_count} · {d.minutes} min</span>
              <em>+{d.growth}</em>
              <form action={deleteDay}>
                <input type="hidden" name="date" value={d.date} />
                <button className="x" title="Remove this day">🗑</button>
              </form>
            </div>
          ))}
        </div>
      </main>
    </>
  );
}
