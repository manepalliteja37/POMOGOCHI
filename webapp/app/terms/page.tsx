import Link from "next/link";

export const metadata = { title: "Terms of Service · Pomogochi" };

export default function Terms() {
  return (
    <main className="page narrow legal">
      <p><Link href="/">← Pomogochi</Link></p>
      <h1>Terms of Service</h1>
      <p className="hint">Last updated: 7 November 2026</p>

      <p>Pomogochi is a free hobby project made by Teja Manepalli for owners of the Pomodoro Rabbit desk timer. By signing in you agree to these simple terms.</p>

      <h2>Using the app</h2>
      <ul>
        <li>Use it for your own focus tracking and fun.</li>
        <li>Only upload day sheet files from your own device.</li>
        <li>Don&apos;t try to break, overload or access other people&apos;s data.</li>
      </ul>

      <h2>No guarantees</h2>
      <p>The app is provided &quot;as is&quot;, free of charge. It may change, have bugs or go offline, and your pet&apos;s data could be lost. Keep the device&apos;s own backups if your history matters to you.</p>

      <h2>Your data</h2>
      <p>Your data stays yours. See the <Link href="/privacy">Privacy Policy</Link>. You can delete everything anytime from the Pet tab.</p>

      <h2>Changes</h2>
      <p>These terms may be updated. The date above shows the latest version.</p>

      <h2>Contact</h2>
      <p>manepalliteja37@gmail.com</p>
    </main>
  );
}
