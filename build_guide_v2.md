## Scope
✅ Weekly report · ✅ Rabbit reacts to time · ✅ ntfy alerts · ✅ Rewards shop
❌ OTA · ❌ Web pause · ❌ Daily goal

## Memory rules
- Art and text go in **flash (PROGMEM)** only. No new IRAM code, no HTTPS.
- After every build we log IRAM / RAM / flash / lowest heap. Targets: **IRAM unchanged**, **lowest heap ≥ 20 KB**.
- **Old saves keep working:** the shop uses the 6 spare bytes already in the pet save, and ntfy adds fields to Settings (v6).

---

## b8 · 📊 Weekly report card (web only)
- A **This week** card: 7 bars of focus minutes (Sun–Sat), total 🍅 and minutes, **best day**, **best task**, streak, fed/petted/played, and ↑↓ vs last week.
- Your phone's browser reads the 7 day files the rabbit already saves, so **no new firmware data**.
- Cost: ~3 KB flash, 0 RAM.

## b9 · 🕰️ Rabbit reacts to time (Home only)
| Time | Rabbit |
|---|---|
| 8–9 am (once) | Ear twitch + *"Good morning!"* |
| 1–2 pm | *"Lunch time?"* |
| 10–11 pm | Tired yawn + *"Getting late"* |
| 11 pm–5 am | Sleep frame + Zzz + dim screen (a button wakes it) |
- Never during focus or break.
- **Priority:** hungry > night > love/happy > normal.
- Reuses the existing frames, ~1 KB flash.

## b10 · 🔔 ntfy alerts
| Alert | When |
|---|---|
| *"Break over, Bunny is waiting 🐰"* | The break ends |
| *"Bunny is hungry 🥕"* | Fullness < 20 in free time, **max once a day** |
| *"Today: 4 🍅 · 100 min · 3 tasks · streak 6"* | Evening, default **9 pm** |
- **Never sent during focus.** If Wi-Fi is down, it retries once after 1 min, then skips.
- Web settings: topic name, 3 on/off toggles, summary time, **Test** button.
- Export leaves out the topic, like the Wi-Fi password.
- Cost: ~3 KB borrowed for <1 s, ~5 KB flash.

## b11 · 🎀 Rewards shop
- **Items:** 6 hats (bow, flower, party hat, scarf, crown, glasses) + 4 scene strips (grass, rain, stars, festival lamps).
- **Prices:** 5–40 each. The crown needs a **30-day streak**, the lamps a **100-day streak**.
- Wear **1 hat + 1 scene** at a time. Buy and wear them on the web page, and the rabbit shows them on Home. Eggs can't wear hats.
- **Art first:** I make a preview sheet for your OK before any code.
- Cost: ~6–8 KB flash, 0 RAM, ~1 ms extra draw.
- **❓ Open: currency.** My pick: **Stars** (25 min = 1, 50 min = 2, task done = +1), so carrots stay food and feeding never fights shopping.

---

## How we build
b8 → b9 → b10 → b11. Each build: compile → you test → lock → next. README + docs are updated every build, and the final zip is tagged **v1.2**.