# 🌐 Web page rules and options

<p align="center"><img src="media/care.gif" width="160" alt="The rabbit being fed and petted"></p>

> The rabbit has its own little web page, served by the device itself. There's no app, no account and no internet needed. Your phone or laptop just has to be on the **same Wi-Fi**.

📚 **All docs:** [🏠 README](../README.md) · [🛠️ Setup](setup.md) · [🐣 Pet rules](petrules.md) · **🌐 Web page rules** (you are here)

**Jump to:** [🔗 Open it](#open) · [📝 To-do](#todo) · [✅ Done today](#done) · [📊 This week](#week) · [📅 History](#history) · [🔥 Streak](#streak) · [🐰 Pet](#pet) · [🪔 Festival](#festival) · [⚙️ Settings](#settings) · [📱 Phone alerts](#ntfy) · [💾 Backup](#backup) · [🩺 Footer](#footer) · [📏 Limits](#limits) · [🧑‍💻 API](#api)

---

<a id="open"></a>

## 🔗 Open the page

| Way | Address |
|---|---|
| ⭐ Easiest | **http://pomodoro.local** |
| Always works | the **IP address** shown on the device: Home → **BACK** (Info screen), e.g. `http://192.168.1.42` |
| First-time Wi-Fi setup | join the `Pomodoro-Setup` hotspot → **http://192.168.4.1** ([setup guide](setup.md#wifi)) |

- 🔄 The page refreshes itself every **2 seconds** while it's open, so changes on the device show up by themselves.
- 💤 While the page is open, the device **won't fall asleep** ([idle sleep](petrules.md#sleep)).
- ⏸️ When the browser tab is hidden, it stops refreshing (saves battery).
- 📱 It's made for phones first, and works on any browser.

The top line shows the **device time (IST)** and its source (NTP or saved), plus **"session Ns left"** while a focus or break runs.

---

<a id="todo"></a>

## 📝 To-do

| Action | How |
|---|---|
| ➕ Add | type in **New task** → **Add** (or press Enter) |
| ✏️ Edit | tap the task text |
| ⬆️⬇️ Reorder | the arrow buttons (done tasks are skipped) |
| ❌ Delete | **×**, then confirm |
| ✅ Mark done | tick the checkbox |

**Tags next to a task**

| Tag | Meaning |
|---|---|
| ↻ **2d** | carried over from earlier days (2 days) |
| 🍅**25** | one 25 min focus spent on it today |
| 🍅**25×2** | two 25 min focuses today |
| 🍅**50** | one 50 min focus today |
| 🍅**3 earlier** | focuses spent on it on earlier days |

> 🥕 Each 🍅25 earned **1 carrot** and each 🍅50 earned **2** (for the first 4 sessions of the day). So the tags tell you how much food you've earned. See [Rewards](petrules.md#rewards).

**Rules**

- 🔢 **7 open tasks** at most. Finish one to add more.
- 📦 **12 tasks in total** per day (open + done). Done ones clear at midnight.
- ✍️ **32 characters** max, **English letters, numbers and symbols only** (the device font has no Telugu or emoji).
- 🌙 At **midnight**: done tasks leave, and unfinished tasks carry over (↻ +1 day).
- 🍅 On the device, **25 and 50 min focuses only start from a task**. 1, 2 and 10 min don't need one.

---

<a id="done"></a>

## ✅ Done today

- Every task finished today, crossed out, with its 🍅 tags.
- **Untick** a task to move it back to open (if you ticked it by mistake).
- It clears at midnight. The day is saved in [History](#history) first.

---

<a id="week"></a>

## 📊 This week (b8)

A weekly report card, Sunday to Saturday.

- **Bars:** focus minutes for each day. **Today is orange**, your **best day is green**, and days still to come are empty.
- **Totals:** 🍅 sessions, focus time and ✔ tasks done this week.
- **Best day**, and **↑ / ↓ %** vs the same days of last week (Sun up to today's weekday, so it's fair mid-week).
- **Best task:** the task with the most focus minutes this week, and how many 🍅 it took.
- **Care:** how many times you fed, petted and played this week.
- Only 25/50 focuses count. Tests and 10 min quick focuses don't.
- Your phone's browser builds the card from [History](#history) and the day sheets, so the device uses no extra memory. Care counts start from b8 (older day sheets don't have them, so they count as 0).

---

<a id="history"></a>

## 📅 History (last 30 days)

Each row: **date · focus sessions · focus minutes · ✔ done tasks · ○ open tasks · JSON**.

- **Today** is the first row and updates live.
- **JSON** downloads that day's **day sheet**: every focus with its time, minutes, task and done flag, plus the done and open tasks, the pet's stats and your streak. It's useful for spreadsheets or a future companion app.
- Only 25/50 focuses count in the session and minute totals. Tests and 10 min quick focuses are listed separately in the JSON.
- The 30 newest days are kept, and older ones are deleted automatically.

---

<a id="streak"></a>

## 🔥 Streak

- 🔥 **Current streak** in days, plus your **best**.
- 🛡️ **"shield ready"** when you hold a streak shield.
- **today N/4**: the 25/50 sessions finished today (4 = the long-break cycle and the daily reward limit).
- **Last 7 days** as ✅ / ❌, oldest on the left, today on the right.
- **Milestones** 3 · 7 · 30 · 100 (✔ once reached).

Full rules: [Streak and shield](petrules.md#streak).

---

<a id="pet"></a>

## 🐰 Pet

- **Stage** (Egg / Baby / Kid / Teen / Adult) · **Growth** points and how many are needed for the next stage · **mood** · 🥕 **carrots held**.
- **Fullness, Love, Energy** bars: 🟢 above 60 · 🟠 30–60 · 🔴 below 30.
- **Today:** 🥕 fed **N** · ❤️ petted **N** · ⚽ played **N**: how many times you used the care menu today (resets at midnight).
- ✏️ **Rename:** 1–11 English characters. The name shows on the device.

Full rules: [Pet rules](petrules.md).

---

<a id="festival"></a>

## 🪔 Festival day

| Button | Does |
|---|---|
| **Tomorrow is a festival** | marks tomorrow |
| 📅 date + **Set** | marks any date you pick |
| **Clear** | removes it |

The card shows **"Today is a festival 🎉"** or **"Next festival: Fri 23 Oct"**. Only **one** festival date is stored at a time, and it clears itself after the day passes. What changes on that day: [Festival rules](petrules.md#festival).

---

<a id="settings"></a>

## ⚙️ Settings

| Option | What it does | Default |
|---|---|:---:|
| 🔇 **Mute focus sounds** | silences the start bell and the heartbeat. The *focus done* chime, break and pet sounds still play. (Same as holding **MODE** on the device.) | off |
| 💓 **Focus heartbeat** | the soft sound during focus: 432 Hz · 216 Hz deep · 528 Hz · **264 Hz deep** · 216 Hz deep slow. Picking one plays a preview beat. | 264 Hz deep |
| 💡 **Backlight** | screen brightness (slider). Changes live. | full |
| ☕ **Break auto-start** | **On:** the break timer starts by itself when focus ends (or right after "Task done?"). **Off:** the break waits on **BREAK READY** until you press OK. | on |
| ⏳ **Show minutes during focus** | **On:** the hourglass plus the mm:ss countdown. **Off:** **hourglass only**, for a calmer focus without watching numbers. Changes live, even mid-focus. | on |
| 👋 **Replay onboarding** | shows the 4 welcome pages on the device again (after any running session) | |
| 📶 **Change Wi-Fi** | forgets the saved Wi-Fi and restarts into the `Pomodoro-Setup` hotspot ([setup step 6](setup.md#wifi)) | |

Below the buttons: the **Wi-Fi name · signal (dBm) · IP address**.

---

<a id="ntfy"></a>

## 📱 Phone alerts (ntfy, b11)

The rabbit can ping your phone through the free **ntfy** app (Android / iPhone). No account needed.

1. Install **ntfy** on your phone, tap **+**, and subscribe to a topic, e.g. `teja-rabbit-4821` (or tap **Random** on the web page and copy it).
2. Type the same topic on the web page and tap **Save**.
3. Tap **Send test**. Your phone should buzz within a few seconds.

| Alert | When |
|---|---|
| ☕ **Break over** | a full break finishes (not when you skip it) |
| 🥕 **Rabbit hungry** | Fullness under 20 in free time, 8 am–10 pm, **at most once a day** |
| ⭐ **Evening summary** | at the hour you pick (18–23, default **21:00**): focuses, minutes, tasks done, streak |

- Each alert has its own on/off box. **Turn off** clears the topic and stops all alerts.
- Never sent during a focus or break. If the rabbit is asleep, Wi-Fi wakes just to send, then sleeps again.
- If ntfy.sh can't be reached, it tries once more after a minute, then drops that alert. The line under the buttons shows the last result.
- 🔒 Anyone who knows your topic can read the alerts, so pick something hard to guess. The topic is **not exported** in backups.

---

<a id="backup"></a>

## 💾 Backup

| Button | Does |
|---|---|
| **Export JSON** | downloads `pomodoro-backup-YYYYMMDD.json`: to-dos, pet, streak and settings |
| **Import JSON** | pick a backup file → confirm → it replaces the device's to-dos, pet, streak and settings |

- 🔒 The **Wi-Fi password and the ntfy topic are never exported**.
- Older backups import fine. Missing new settings keep their defaults.
- Updating the firmware with **Erase Flash = Only Sketch** keeps everything anyway. Export before a **full erase** or a new board.

---

<a id="footer"></a>

## 🩺 Footer (health line)

`v1.1-b7 · up 3h 58m · last reset: Power On · heap 31296 B (lowest 29520) · slowest loop 883 ms in rollover at 00:00`

| Part | Meaning |
|---|---|
| `v1.1-b6` | firmware version |
| `up` | time since the last power-on or reset |
| `last reset` | why it last started: Power On, Software restart, Watchdog, Exception… |
| `heap` / `lowest` | free memory now / lowest seen. Above **20000 B** is healthy |
| `slowest loop … in … at …` | the longest pause seen since boot, which part caused it, and the clock time it happened. Under ~100 ms is normal. One-off spikes happen when the device writes the day sheet at midnight (`rollover`) or switches Wi-Fi off to sleep (`screen`). **`wifi/system`** means none of the rabbit's own code was slow; the Wi-Fi stack paused it, which is harmless |

If something feels wrong, copy this line into a GitHub Issue.

---

<a id="limits"></a>

## 📏 Limits at a glance

| Thing | Limit |
|---|---|
| Open tasks | 7 |
| Tasks per day (open + done) | 12 |
| Task length | 32 characters, English only |
| Pet name | 1–11 characters, English only |
| History | 30 days |
| Festival dates stored | 1 |
| Backlight | 30–1023 |

Every limit is also checked by the device itself, so a wrong request gets a clear error message instead of breaking anything.

---

<a id="api"></a>

## 🧑‍💻 API (for tinkerers)

All answers are JSON. Every POST answers with the full new state, so the page redraws at once.

| Endpoint | Method | Purpose |
|---|:---:|---|
| `/` | GET | the page |
| `/api/state` | GET | everything the page shows: time, Wi-Fi, health, to-dos, today's focus log, care counts, pet, streak, settings, session |
| `/api/todo` | POST | `op=add&t=` · `edit&i=&t=` · `del&i=` · `up&i=` · `down&i=` · `tick&i=&v=0/1` · `clear` |
| `/api/set` | POST | `k=` `mu` mute · `hb` heartbeat 0–4 · `bl` backlight · `ba` break auto-start · `fd` focus minutes 0/1 · `ob` replay intro · `name` · `fest` (`YYYYMMDD`, `t` = tomorrow, `0` = clear) · `nt` ntfy topic (empty = off) · `nf` alert bits (1 break, 2 hungry, 4 summary) · `nh` summary hour 18–23 · `ntest` send a test, with `&v=` the value |
| `/api/wifi` | POST | forget Wi-Fi → setup hotspot |
| `/api/export` | GET | backup download |
| `/api/import` | POST | restore (the page sends the backup's fields) |
| `/api/hist` | GET | today + the last 30 days summary |
| `/api/day?d=YYYYMMDD` | GET | one day sheet download |

Example: `curl -X POST http://pomodoro.local/api/todo -d "op=add&t=Edit video"`

---

📚 **Next:** [🐣 Pet rules](petrules.md) · [🛠️ Build it yourself](setup.md) · [🏠 Back to README](../README.md)
