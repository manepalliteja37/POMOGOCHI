# 🐰 Pomogochi

**Pomodoro Rabbit**: a desk Pomodoro timer with a virtual pet bunny.

It's built on an **ESP8266 NodeMCU** and a **1.8" ST7735 TFT**.
Every focus session you finish feeds and grows your rabbit, from an egg to an adult. Skip work, and it gets hungry, tired or sad. It never dies, and a missed day costs only 1 Growth point (it never shrinks a stage).

> Firmware version: **v1.1-b5** · Board: NodeMCU 1.0 (ESP-12E) · Arduino core: ESP8266 3.1.2

---

## ✨ Features

- **Pomodoro presets:** 25 min (→ 5 min break) and 50 min (→ 10 min break), plus 1 and 2 min test presets. Every 4th focus earns a **15 min long break**.
- **Animated hourglass** during focus, with the sand volume drawn correctly.
- **Soothing sounds:** a start bell, a soft **heartbeat loop** during focus (264 Hz, 50 bpm) and a "focus done" chime. Hold MODE to mute the focus sounds (bell + heartbeat); the focus-done, break and pet sounds still play.
- **Virtual rabbit pet** with five stages (Egg → Baby → Kid → Teen → Adult) and Fullness, Love and Energy stats.
- **Care menu:** Feed, Pet and Play.
- **Streaks:** daily streak, best streak, a 7-day **streak shield**, and milestones at 3, 7, 30 and 100 days.
- **To-do-linked focus:** pick a task on the device, focus on it, then answer **"Task done?"** (OK yes / BACK not yet). Up to 7 open tasks (32 characters each). Done tasks leave the screen and show under **Done today** on the web page. Unfinished tasks carry over to the next day.
- **History:** a log of every finished focus, the last **30 days** on the web page, and a **JSON day sheet** download for each day.
- **Resume after reset or power cut:** if the board resets mid-session, the focus or break carries on exactly. After a power cut (or pulling the USB), it carries on from the last snapshot, saved every minute, so at most ~1 min of focus is lost. The time without power never counts.
- **Built-in web page:** to-dos, Done today, history, pet stats, streak, settings, Wi-Fi reset and JSON **export/import**. Open it at `http://pomodoro.local` or the IP shown on the device.
- **Wi-Fi setup hotspot** (`Pomodoro-Setup`) with a captive portal, so no hard-coded passwords.
- **NTP time (IST)** with a saved-time fallback when offline, and an automatic midnight rollover.
- **Idle sleep:** after 1 hour with no button press (and no session running) the rabbit falls asleep with a 💤 animation. The backlight dims and Wi-Fi turns off. Any button wakes it on the Setup screen, and Wi-Fi and the time resync in a few seconds. An open web page keeps it awake.
- **Festival day:** mark a date on the web page, and that day has no penalties.
- **Crash-safe saves:** two checksummed slots on LittleFS, so a power cut never loses your pet. Re-flashing keeps your data too.

---

## 🔧 Hardware

| Part | Notes |
|---|---|
| ESP8266 NodeMCU v2/v3 | 4 MB flash |
| 1.8" ST7735 TFT, 128×160 | "black tab" module, used in portrait |
| Passive buzzer | driven through a BC547 transistor |
| 3 × push buttons | MODE, OK, BACK |
| BC547 NPN + 1 kΩ base resistor + 1N4148 diode | buzzer driver |
| 10 kΩ resistor | external pull-up for BACK (D0) |
| USB cable | power |

### Wiring

| Signal | NodeMCU pin | Notes |
|---|---|---|
| TFT CS | D8 | |
| TFT DC (A0) | D3 | |
| TFT RST | RST | tied to the NodeMCU RST pin (frees D4 for the backlight) |
| TFT SDA (MOSI) | D7 | hardware SPI |
| TFT SCK | D5 | hardware SPI |
| TFT LED (backlight) | D4 | PWM brightness |
| TFT VCC | 3V3 | ⚠️ don't short it to LED |
| Buzzer | D6 → 1 kΩ → BC547 base | buzzer on the collector, with a 1N4148 diode across it |
| MODE button | D1 → GND | internal pull-up |
| OK button | D2 → GND | internal pull-up |
| BACK button | D0 → GND | **needs an external 10 kΩ pull-up to 3V3** |

> D6 is also SPI MISO. The firmware starts the buzzer *after* the display, so the SPI driver doesn't take over the pin.

---

## 💻 Software setup

1. Install the **Arduino IDE** and add the ESP8266 boards URL:
   `https://arduino.esp8266.com/stable/package_esp8266com_index.json`
2. Boards Manager → install **esp8266 by ESP8266 Community** (tested with 3.1.2).
3. Library Manager → install **Adafruit GFX Library** and **Adafruit ST7735 and ST7789 Library**.
4. Open `pomodoro_rabbit_v1/pomodoro_rabbit_v1.ino`.
5. Tools menu:
   - Board: **NodeMCU 1.0 (ESP-12E Module)**
   - Flash Size: **4MB (FS:2MB OTA:~1019KB)**
   - Erase Flash: **Only Sketch** (keeps your pet, to-dos and Wi-Fi when you update)
6. Upload, then open the Serial Monitor at **115200** baud if you want logs.

Command-line build:

```bash
arduino-cli compile -b esp8266:esp8266:nodemcuv2:eesz=4M2M pomodoro_rabbit_v1
```

Build size (v1.1-b5): flash 42%, RAM 53%.

---

## 📶 First boot

1. A 4-page intro walks you through the buttons. OK moves to the next page, BACK goes to the previous one.
2. If no Wi-Fi is saved, the device opens the **`Pomodoro-Setup`** hotspot. Connect your phone, and the setup page pops up (or open `http://192.168.4.1`). Pick your network and enter the password.
3. Once online, the time syncs from NTP. Press **BACK on Home** to see the Wi-Fi name and the web address.

---

## 🎮 Controls

| Screen | MODE | OK | BACK |
|---|---|---|---|
| **Home** | Setup (presets) | Open the care menu | Info (Wi-Fi + web address) |
| **Care menu** | Next item | Do it | Close (also closes itself after 10 s) |
| **Setup** | Next preset; after all 4 → To-do. For a picked task: 25 ↔ 50 only | Start focus | Home (To-do when a task is picked) |
| **Focus** | — | Pause / resume | Press twice to cancel |
| **Break** | — | Pause / resume | Press twice to skip |
| **To-do** | Home | Focus on this task (opens Setup) · **hold** = done now | Next task |
| **Task done?** | | Yes, done | Not yet (2 min = not yet) |
| **Info** | Home | Replay the intro | Home |
| **Sleeping** | Wake → Setup | Wake → Setup | Wake → Setup |
| **Anywhere** | **Hold** = mute / unmute | | |

Buttons are debounced (30 ms). A long press is 1 s, and a double press of BACK must come within a 2 s confirm window.

---

## 🐣 Pet rules

All numbers live in one file, `petrules.h`, so they're easy to tune.

Every day starts at **Fullness 40, Love 40, Energy 60** at 8 am, so there's something to look after every day.

### Growth

| Stage | Growth points |
|---|---|
| Egg | 0 (hatches after your first completed focus) |
| Baby | 1–9 |
| Kid | 10–39 |
| Teen | 40–99 |
| Adult | 100+ |

- Completed 25 min = **+1 Growth**, 50 min = **+2 Growth** (max 8 per day).
- A day with no completed 25/50 focus costs **−1 Growth**, but it never drops below the start of the current stage (no un-hatching, no shrinking). The egg and festival days are exempt.

### Rewards

| Event | Effect |
|---|---|
| Completed 25/50 min focus | **25 = +1 carrot, 50 = +2 carrots** (max 9 held), Growth, and the streak day marked |
| Full break taken | **+20 Energy** (breaks are rest time: the rabbit sleeps) |
| Daily cap | carrots and Energy rewards for the first 4 sessions only |

### Care menu

| Action | Effect | Limits |
|---|---|---|
| **Feed** | 1 carrot → **+10 Fullness, +10 Energy** (munch sound) | needs a carrot |
| **Pet** | **+5 Love** (happy sound) | 3 times per day, not at full Love |
| **Play** | **+5 Love**, −10 Energy (play sound) | not while Sick or under 20 Energy |

### Gentle penalties

| Event | Effect |
|---|---|
| Cancel in the first 2 min | nothing (grace period) |
| Cancel after 2 min | −5 Love |
| 3 such cancels in a day | the rabbit is **Worried** until midnight |
| Skip a break | no Energy reward, and −5 Energy |
| 8 am every day | fresh start: **Fullness 40, Love 40, Energy 60** |
| Daytime (8 am–11 pm) | Fullness drops every hour: −5, then −4 / −3 / −2 after 1 / 2 / 3 rewarded focuses, and **none after 4**. −5 Love every 2 h. The egg is exempt |
| A day with no completed focus | streak resets (unless a shield saves it) and **−1 Growth** (never below the stage's start) |
| Fullness at 0 for 2 nights | the rabbit is **Sick** (grey). 2 completed sessions cure it |

### Festival day 🪔

On the web page, tap **Tomorrow is a festival** (or pick a date). On that day there's no hourly hunger or love drop, no cancel or skip penalty, no streak break and no Growth loss. Focus still counts. The 8 am fresh start (F40 L40 E60) still happens, and the next day carries on normally with your streak intact.

### Streak

- A day counts with at least one completed 25 or 50 min focus.
- Every 7 streak days earns a **shield** (max 1), which saves one missed day.
- Milestones at 3, 7, 30 and 100 days.

---

## 🌐 Web page & API

Open `http://pomodoro.local` (or the device IP) on the same Wi-Fi.

| Endpoint | Method | Purpose |
|---|---|---|
| `/` | GET | the web page |
| `/api/state` | GET | pet, streak, to-dos and settings as JSON |
| `/api/todo` | POST | add / edit / delete / move / tick to-dos |
| `/api/set` | POST | mute, heartbeat sound, backlight, break auto-start, pet name, replay intro, festival day (`k=fest&v=YYYYMMDD`, `t` = tomorrow, `0` = clear) |
| `/api/wifi` | POST | forget Wi-Fi and reopen the setup hotspot |
| `/api/export` | GET | backup as JSON (the Wi-Fi password is never exported) |
| `/api/import` | POST | restore a backup |
| `/api/hist` | GET | today + the last 30 days (sessions, minutes, done, open) |
| `/api/day?d=YYYYMMDD` | GET | one day sheet as a JSON download (today is built live) |

---

## 📁 Project structure

```
pomodoro_rabbit_v1/
├── pomodoro_rabbit_v1.ino   main loop, serial status
├── storage.h                crash-safe A/B save files (LittleFS + CRC32)
├── netsetup.h               Wi-Fi join + captive setup hotspot
├── clock.h                  NTP time (IST) + saved fallback
├── buzzer.h                 chimes + focus heartbeat (non-blocking)
├── input.h                  3-button layer (debounce, long, double press)
├── appdata.h                settings, pet, to-dos, dirty-flag saving
├── webui.h                  web page + JSON API
├── history.h                day log, day sheets, 30-day history, carry-over
├── gfx.h                    display helpers, top bar, sprites
├── session.h                focus / break engine, presets, cycle dots
├── hourglass.h              hourglass animation
├── petrules.h               all pet rules and numbers
├── screens.h                every screen, button map, care menu
├── pet_frames.h             1-bit rabbit sprites (generated)
└── LICENSE_bunny_art.txt    MIT notice for the rabbit art
```

`tools/gen_frames.py` regenerates `pet_frames.h` from the PNG art (the argument is the threshold, default 190).

---

## 🩺 Serial monitor

At 115200 baud, type `?` for a status report: version, uptime, last reset reason, lowest free heap, time, Wi-Fi, pet stats, streak and to-dos.

For testing, set `#define TEST_KEYS 1` at the top of the `.ino` to enable extra keys: fast mode (1 min = 1 s), "pretend a night passed", add a carrot, reset the pet, and more.

---

## 🙏 Credits

- Rabbit artwork from [limengdu/reTerminal_Sticky_Bunny](https://github.com/limengdu/reTerminal_Sticky_Bunny), MIT License, © 2026 limengdu. See `LICENSE_bunny_art.txt`.
- Built with the Adafruit GFX and ST7735 libraries and the ESP8266 Arduino core.

Made by **Teja Manepalli** 🇮🇳
