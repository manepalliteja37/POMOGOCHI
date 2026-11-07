import Link from "next/link";

export const metadata = { title: "Privacy Policy · Pomogochi" };

export default function Privacy() {
  return (
    <main className="page narrow legal">
      <p><Link href="/">← Pomogochi</Link></p>
      <h1>Privacy Policy</h1>
      <p className="hint">Last updated: 7 November 2026</p>

      <p>Pomogochi is a free hobby app made by Teja Manepalli. It lets you grow a virtual pet from the day sheets your Pomodoro Rabbit timer creates. This page explains what it keeps and why.</p>

      <h2>What we collect</h2>
      <ul>
        <li><b>Your Google account basics:</b> your email address, name and profile picture, which Google shares when you sign in. We only use them to know it&apos;s you.</li>
        <li><b>Your pet:</b> the animal you picked and the name you gave it.</li>
        <li><b>Day sheets you upload:</b> the date, focus sessions, minutes, streak and the tasks you marked done.</li>
      </ul>
      <p>We don&apos;t read your Gmail, contacts, Drive or anything else in your Google account. We don&apos;t use ads, trackers or analytics.</p>

      <h2>How it&apos;s used</h2>
      <p>Only to show your pet, its growth and your history to you. Nothing is sold or shared with anyone.</p>

      <h2>Where it&apos;s stored</h2>
      <p>Data is stored in a Supabase database (Mumbai region) and the app is hosted on Vercel. Row-level security means each account can only see its own data.</p>

      <h2>Deleting your data</h2>
      <p>Open <b>Pet → Start over → Delete everything</b> to erase your pet and every uploaded day straight away. To remove your account completely, email us and it will be deleted within 7 days.</p>

      <h2>Contact</h2>
      <p>manepalliteja37@gmail.com</p>
    </main>
  );
}
