## 1. How the parts talk to each other
```
Buttons ─► Input layer ─► Events ─► State machine ─► Display / Buzzer / Storage
Web page (Wi-Fi) ─► Storage ─► State machine (screen refresh)
millis() tick ─► Timer ─► State machine
NTP clock ─► Midnight IST rollover ─► Streak, decay, daily counters
```
- **Input layer:** turns noisy button signals into clean **SHORT / LONG / DOUBLE** events.
- **State machine:** one active state at a time. An event goes only to that state.
- **Display:** redraws only the parts that changed.
- **Buzzer:** plays without blocking anything.
- **Storage:** writes only when something changes.
- **Web server:** runs in the background and never blocks the timer.

## 2. Buttons and noise
### What polling means
There are two ways to read a button:
- **Interrupt:** the pin interrupts the chip on every edge. A bouncing contact fires it 10–50 times per press, and D0/GPIO16 has no interrupt support at all.
- **Polling (our choice):** the main loop looks at each pin on a fixed schedule and decides for itself.

### Why every 5 ms
- A human press lasts **80–300 ms**. Switch bounce lasts **5–20 ms**.
- Sampling every 5 ms gives 16+ readings per press, so a press is never missed.
- **Debounce:** the button needs **6 identical readings in a row (30 ms)**. Bounce never stays stable that long; a real press does.
- **Cost:** 3 pin reads every 5 ms is under 0.1% CPU. Wi-Fi is unaffected.
- 10 ms would work too. 5 ms makes the LONG and DOUBLE timing tighter.

### Rules
| Rule | Value |
|---|---|
| Debounce | stable for 30 ms |
| Boot lockout | ignore buttons for the first 500 ms |
| SHORT | press + release in < 700 ms |
| LONG | held ≥ 1000 ms, fires once while held, release ignored |
| DOUBLE (BACK) | 2 SHORTs, gap ≥ 120 ms (shorter = bounce), 2nd within 2 s |
| 2 buttons at once | first stable press wins |

- **Confirm prompt:** the 1st BACK shows "Press BACK again to cancel" for 2 s. A 2nd press confirms.
- **MODE LONG** toggles mute. **MODE SHORT** is ignored during Focus and Break.

## 3. Boot
1. Splash, then Wi-Fi + NTP start in the background.
2. Load settings, to-dos, pet, streak and the last epoch from flash.
3. If the day changed, run the midnight rollover (decay, streak check).
4. Not onboarded → Onboarding. Otherwise → Home.

## 4. Re-flashing firmware: what survives
- **LittleFS is a separate flash area.** Uploading a new sketch rewrites only the program area. Your pet, streak, to-dos, settings and Wi-Fi all survive.
- They are lost only if:
  - Arduino IDE **Tools → Erase Flash** is set to "All Flash Contents"
  - you upload a LittleFS data image
  - you change the Flash Size / FS partition setting
- Keep Erase Flash on **"Only Sketch"**.
- **Data version number:** every save file carries one, so new firmware upgrades old data instead of wiping it.
- **The web page has no data of its own.** It reads from the device, so after a re-flash it shows the same values.
- **Backup:** Export (download JSON) and Import (restore) buttons on the web page.

## 5. Onboarding
Shown on first boot, and can be replayed from the web page. OK = next, BACK = previous.
1. **Welcome:** egg in its nest, "Focus to grow your rabbit."
2. **Buttons:** short, long and double press.
3. **Rewards:** finish a 25/50 min focus = food, take the full break = energy.
4. **Wi-Fi:** status + IP. OK → Home, where the **egg is waiting**: "Finish your first focus to hatch me!"

## 6. Home screen layout (128 × 160 portrait)
```
y=0   ┌──────────────────────────┐
      │ 14:32      WiFi 🔇  🔥12 │  Top bar      14 px
y=14  ├──────────────────────────┤
      │  "Ready to focus?"       │  Speech line  16 px
y=30  ├──────────────────────────┤
      │                          │
      │      RABBIT 96 × 96      │  Pet stage    96 px
      │                          │
y=126 ├──────────────────────────┤
      │ 🥕 ▓▓▓▓▓▓▓░░░             │  Fullness
      │ ♥  ▓▓▓▓▓░░░░░             │  Love         3 × 10 px
      │ ⚡ ▓▓▓▓▓▓▓▓░░             │  Energy
y=160 └──────────────────────────┘
```
| Zone | Rows | Contents |
|---|---|---|
| Top bar | 0–13 | **Left:** time HH:MM (IST), "--:--" if never synced. **Right:** Wi-Fi icon (solid / blinking / crossed), mute icon (only when muted), flame + streak days. |
| Speech | 14–29 | One line, about 20 characters, set by mood or event: "I'm hungry", "Great focus!", "Day 7 streak!" |
| Pet stage | 30–125 | Rabbit centred. Size depends on the stage. Idle blink, ear twitch or look-around every 3–8 s. |
| Stats | 126–159 | 3 bars, 0–100: 8 px icon + 96 px bar. Green > 60, amber 30–60, red < 30. |

- **OK** = care menu (designed in the pet step). **MODE** = Pomodoro.
- **Redraw:** top bar once a minute, stage only on animation frames, bars only when a value changes.

## 7. Egg → adult growth
- **Growth points** come only from completed focus: **25 min = +1, 50 min = +2**.
- **Growth never goes down.** No penalty touches it.

| Stage | Growth | Sprite | Unlocks | Typical time* |
|---|---|---|---|---|
| Egg | 0 | 48 px, wobbles | — | until 1st focus |
| Baby | 1–9 | ~56 px | Feed | day 1 |
| Kid | 10–39 | ~68 px | Pet | ~3–5 days |
| Teen | 40–99 | ~80 px | Play | ~2–3 weeks |
| Adult | 100+ | 96 px | Outings, extra idle moves | ~1–1.5 months |

*At 2–4 sessions a day. The daily cap is 8 growth.

- **Hatch:** the first completed 25/50 cracks the egg, with an animation and a hatch chime → Baby.
- **Stage up:** checked at the end of each focus. Sparkle, jingle and "I grew!"
- **Art:** the repo's frames. If it has no separate baby or teen art, size shows the stage. Exact frames are picked in the bitmap step.

## 8. Navigation
**Home → MODE → Pomodoro Setup → MODE → To-do → MODE → Home**
- On Setup, MODE changes the preset. BACK → Home.
- While a timer runs, only the mute long press works.

## 9. Pomodoro
### What a preset is
A **preset** is a ready-made timer choice, so you never set minutes by hand. MODE flips through them on the Setup screen, and the last one used is remembered.

| Preset | Focus | Break after | Counts? |
|---|---|---|---|
| 1 min | 1 | none | No (test) |
| 2 min | 2 | none | No (test) |
| 25 min | 25 | 5 min | Yes |
| 50 min | 50 | 10 min | Yes |

### Cycle dots ●●○○
- 4 dots on the Setup screen show progress toward the **15-min long break**.
- **●** = a completed 25/50 in this cycle. **○** = still to go.
- ○○○○ → ●○○○ → ●●○○ → ●●●○ → ●●●● → **15-min long break** → back to ○○○○
- 1/2-min tests and cancelled sessions never fill a dot. The dots reset at midnight IST.

### Flow
1. **Setup:** preset, today's count, cycle dots. MODE = next preset, OK = start, BACK = Home.
2. **Focus:** amber sand, soft chime every 30–60 s. OK = pause/resume. BACK ×2 = cancel (see penalties).
3. **Complete (25/50):** chime, rabbit cheers, +1 food, +Growth, a dot filled, the streak day marked. A 1/2-min test gives a chime only → Setup.
4. **Break auto-starts:** 15 min if all 4 dots are full, otherwise 5 or 10 min.
5. **Break:** green sand, sleeping rabbit, silent. OK = pause. BACK ×2 = skip.
6. **Break done:** wake chime, +1 Energy → Setup waits for OK.

A reboot mid-session loses that session.

## 10. To-do
### Storage
- **Up to 7 items.** The web page refuses an 8th ("List full, 7/7").
- **Each item holds up to 32 characters.** The web input enforces the limit.
- **English / ASCII only.** The device font has no Telugu letters, so Telugu would show as boxes.
- Each item = its text + a done flag, saved in LittleFS. Survives reboots and re-flashing.

### Screen
```
┌──────────────────────────┐
│ TO-DO            3/7 done│  Header  14 px
├──────────────────────────┤
│  ☐ Solder buzzer         │
│ ▶☑ Write script ep 12   │  7 rows × 18 px
│  ☐ Order spare TFT       │
│  …                       │
├──────────────────────────┤
│ OK=tick  MODE=next screen│  Footer  20 px
└──────────────────────────┘
```
- About **18 characters show** per row. The **highlighted row scrolls** a longer item so you can read it all.
- **Done** = ticked box + grey text. It stays until you clear it on the web page.
- **Controls:** BACK = move the highlight down (wraps around), OK = tick/untick, MODE = next screen.
- **Empty list:** "No tasks. Add at http://<IP>"
- **Web page:** add, edit, delete, reorder, tick, clear done. Changes appear on the device within about 1 s.

## 11. Rewards, penalties, streak
Penalties are mild nudges, never punishment. The rabbit never dies and never loses growth.

### Rewards
| Event | Effect |
|---|---|
| Complete 25 min | +1 food, +1 Growth, streak day marked |
| Complete 50 min | +1 food, +2 Growth, streak day marked |
| Full break | +1 Energy |
| Daily cap | rewards for the first 4 sessions only |

### Penalties
| Event | Effect |
|---|---|
| Cancel within the first 2 min | None (grace period for a wrong start) |
| Cancel after 2 min | −5 Love, sad face, "Aww…" |
| 3+ cancels in a day | Rabbit is **Worried** for the rest of the day |
| Skip a break | No Energy, plus −5 Energy, "I'm tired" |
| A day with zero 25/50 | Streak resets, Fullness −25 |
| Fullness at 0 for 2 days | Rabbit is **Sick** (grey tint, no Play). 2 completed sessions cure it |

### Streak
- A day counts with **≥ 1 completed 25/50**.
- A missed day sets the current streak to 0. The best streak is kept.
- **Streak shield:** earned at the 7-day milestone (you can hold 1). It saves one missed day automatically.
- Milestones at **3, 7, 30, 100** days trigger a celebration.
- Uses NTP time, or the saved clock when offline.

*All numbers are starting values, to be tuned in testing.*

## 12. Web page
- **To-dos:** add, edit, delete, reorder, tick, clear done (shows the 7 / 32 limits)
- **Streak:** current, best, shield, today's sessions, last 7 days ✓/✗, milestones
- **Pet:** name, stage, Growth, stats, mood
- **Settings:** Wi-Fi, mute, volume, backlight, break auto-start, replay onboarding
- **Backup:** Export / Import JSON

## 13. Saving
- Pet + streak: 2 checksummed slots, saved on change.
- Clock epoch: every 10 min.
- Settings: last preset, today's count, cycle dots, mute, volume, backlight.
- Every file carries a data version, so a re-flash keeps everything.