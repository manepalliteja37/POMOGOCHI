**How each stage works:** one small test sketch → flash → check the pass criteria → lock → next stage.
Every sketch prints its free heap to Serial (115200). The heap must stay **≥ 20 KB**.

## Stage 0 — Setup + display bring-up
0.1 Arduino IDE + ESP8266 core, board **NodeMCU 1.0**, Erase Flash = **Only Sketch**, LittleFS 1–2 MB
0.2 Pick the display library once (Adafruit_ST7735 or TFT_eSPI)
0.3 Test: red/green/blue fills, text and a rectangle in portrait
0.4 Test: backlight PWM on D4, fading 0 → 100%

✅ **Pass:** true colours (no RGB/BGR swap), no edge garbage, upright text, smooth dimming.

## Stage 1 — Timer engine
1.1 `millis()` countdown with start, pause, resume, cancel and remaining time. Must be rollover-safe.
1.2 Control it over Serial (s = start, p = pause, c = cancel) using the 1-min preset
1.3 Show MM:SS on the TFT, redrawing only the digits that change
1.4 Session chain: focus → automatic break → idle
1.5 Cycle-dot logic: only 25/50 count, and the 4th gets the long break (tested with a 20-second stand-in for 25 min)

✅ **Pass:** a 25-min run is within 2 s of a phone stopwatch, pause holds exactly, no flicker.

## Stage 2 — Hourglass
2.1 Static hourglass outline
2.2 Sand level from √(remaining ÷ total): the top shrinks as the bottom pile grows
2.3 Falling stream in the neck, redrawn in small pieces only
2.4 Amber sand for focus, green for break
2.5 Connect it to the Stage 1 timer

✅ **Pass:** smooth at both 1 min and 25 min, no full-screen flashes.

## Stage 3 — Bitmaps + animation
3.1 Pick frames: idle, blink, ear twitch, happy, sad, sleep, eat, egg, egg crack (keep the MIT notice)
3.2 Convert on the PC: downscale → 1-bit (compare dithering with a plain threshold) → PROGMEM C arrays
3.3 Draw one 96×96 sprite and time how long it takes
3.4 Non-blocking frame player (list of frames + delay per frame)
3.5 Check the stage sizes: 48 / 56 / 68 / 80 / 96 px
3.6 Measure how much flash the art uses

✅ **Pass:** the rabbit is clear at every size, a frame swaps in under 30 ms, flash use is within budget.

## Stage 4 — Buttons
4.1 Print the raw pin states (confirm D0's pull-up works)
4.2 5 ms polling + 30 ms debounce → SHORT
4.3 LONG on MODE (≥ 1 s, fires once)
4.4 DOUBLE on BACK (≥ 120 ms gap, 2 s window) + the confirm prompt
4.5 500 ms boot lockout, plus the two-buttons-at-once rule
4.6 **Noise torture test:** 50 fast taps, 20 holds, wiggly presses, a button held through boot

✅ **Pass:** zero false doubles, zero missed presses, zero ghost long presses.

## Stage 5 — Buzzer
5.1 A single tone through the BC547 on D6
5.2 Non-blocking melody player for every chime: focus tick, done, break start and end, hatch, stage-up
5.3 Mute and volume

✅ **Pass:** chimes never stall the timer or the animation, and mute silences everything.

## Stage 6 — Storage + time
6.1 LittleFS write and read, surviving both a reset and a re-flash
6.2 Versioned, checksummed files, with 2 save slots for the pet
6.3 Wi-Fi connect, with a hotspot setup page the first time
6.4 NTP in IST, epoch saved every 10 min, offline boot from the saved epoch
6.5 Midnight rollover, tested with a faked clock

✅ **Pass:** data survives 10 power cuts in the middle of writes, and the time is right after an offline reboot.

## Stage 7 — Web page
7.1 One small HTML page served from flash
7.2 To-do actions (add, edit, delete, reorder, tick) with the 7 × 32 limits
7.3 Streak, pet and settings views, plus replay onboarding
7.4 Export / Import JSON

✅ **Pass:** a web edit shows on the device within 1 s, the timer is unaffected while the page loads, heap stays ≥ 20 KB.

## Stage 8 — Screens + pet logic
8.1 State machine skeleton covering all states
8.2 Onboarding's 4 pages
8.3 Home layout
8.4 To-do screen
8.5 Pet rules: rewards, penalties, growth stages, streak + shield, moods
8.6 Care menu (designed in the pet step)

✅ **Pass:** every flow in the spec can be walked through using only the buttons.

## Stage 9 — Final firmware
9.1 Merge the locked modules into one project, one file per module
9.2 Full-day test: 4+ real sessions, web edits, a Wi-Fi drop, a reboot
9.3 Tune the numbers (decay, penalties, chime timing)
9.4 Tag it as **v1.0**

✅ **Pass:** runs 24 h with no crash or reset, and the heap stays steady.

## Rules for every stage
- One sketch per small task. Never skip ahead.
- Report what happened, with the Serial log or a photo. We fix it, then lock it.
- A locked module is reused as-is in every later stage.