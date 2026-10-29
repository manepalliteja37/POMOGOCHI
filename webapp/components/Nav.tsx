import Link from "next/link";

export default function Nav({ active }: { active: "home" | "upload" | "history" | "settings" }) {
  const items = [
    ["home", "/home", "🏠", "Home"],
    ["upload", "/upload", "📤", "Upload"],
    ["history", "/history", "📅", "History"],
    ["settings", "/settings", "⚙️", "Pet"],
  ] as const;
  return (
    <nav className="nav">
      <Link href="/home" className="brand">🐾 Pomogochi</Link>
      <div className="tabs">
        {items.map(([k, href, icon, label]) => (
          <Link key={k} href={href} className={k === active ? "tab on" : "tab"}>
            <span>{icon}</span> {label}
          </Link>
        ))}
      </div>
    </nav>
  );
}
