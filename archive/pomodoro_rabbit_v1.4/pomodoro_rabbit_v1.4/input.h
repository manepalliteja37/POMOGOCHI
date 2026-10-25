// input.h - button input layer (Stage 4 LOCKED, copied as-is from stage5_buzzer)
// MODE D1, OK D2, BACK D0 (ext 10k pull-up). Poll every 5 ms, 30 ms debounce,
// SHORT < 700 ms, LONG >= 1 s, BACK DOUBLE (gap >= 120 ms, within 2 s).
// Use: inputBegin(); call inputPoll() every loop(); while (popEv(e)) ...
#pragma once
#include <Arduino.h>

enum BtnId : uint8_t { B_MODE, B_OK, B_BACK, B_COUNT };
enum EvType : uint8_t { EV_NONE, EV_SHORT, EV_LONG, EV_DOUBLE, EV_CONFIRM_TIMEOUT };
struct Event { EvType type; BtnId btn; };

const uint8_t  BTN_PIN[B_COUNT]  = { D1, D2, D0 };
const char*    BTN_NAME[B_COUNT] = { "MODE", "OK", "BACK" };

const uint16_t POLL_MS        = 5;
const uint8_t  STABLE_READS   = 6;      // 6 x 5 ms = 30 ms
const uint16_t BOOT_LOCK_MS   = 500;
const uint16_t SHORT_MAX_MS   = 700;
const uint16_t LONG_MS        = 1000;
const uint16_t DOUBLE_GAP_MS  = 120;
const uint16_t DOUBLE_WIN_MS  = 2000;

struct Btn {
  bool     stable;      // true = pressed (debounced)
  bool     lastRaw;
  uint8_t  same;        // equal raw reads in a row
  bool     armed;       // seen released after boot lockout
  bool     owner;       // this press got accepted (first-press-wins)
  bool     longFired;
  uint32_t downAt, upAt;
  uint16_t rawEdges;    // bounce counter for the current press
};
Btn btn[B_COUNT];

// confirm window for BACK double
bool     confirmOpen = false;
uint32_t confirmFirstUp = 0;

// event queue (tiny ring)
Event    evq[8]; uint8_t evHead = 0, evTail = 0;
void pushEv(EvType t, BtnId b) { uint8_t n = (evHead + 1) & 7; if (n != evTail) { evq[evHead] = {t, b}; evHead = n; } }
bool popEv(Event& e) { if (evTail == evHead) return false; e = evq[evTail]; evTail = (evTail + 1) & 7; return true; }

// test counters
struct Cnt { uint16_t shorts, longs, doubles, midDrops, rejected, bounceGaps, maxEdges; } cnt[B_COUNT];
bool rawMode = false;

bool readPressed(uint8_t i) { return digitalRead(BTN_PIN[i]) == LOW; }

void inputBegin() {
  pinMode(D1, INPUT_PULLUP);
  pinMode(D2, INPUT_PULLUP);
  pinMode(D0, INPUT);                       // external 10k pull-up
  for (uint8_t i = 0; i < B_COUNT; i++) {
    bool p = readPressed(i);
    btn[i] = { p, p, 0, false, false, false, 0, 0, 0 };
  }
}

bool anotherOwner(uint8_t me) {
  for (uint8_t i = 0; i < B_COUNT; i++) if (i != me && btn[i].stable && btn[i].owner) return true;
  return false;
}

void onStablePress(uint8_t i, uint32_t now) {
  Btn& b = btn[i];
  b.downAt = now; b.longFired = false;
  if (!b.armed || anotherOwner(i)) { b.owner = false; if (b.armed) cnt[i].rejected++; return; }
  b.owner = true;
}

void onStableRelease(uint8_t i, uint32_t now) {
  Btn& b = btn[i];
  b.upAt = now;
  if (b.rawEdges > cnt[i].maxEdges) cnt[i].maxEdges = b.rawEdges;
  if (rawMode) Serial.printf("  %s released, raw edges this press: %u\n", BTN_NAME[i], b.rawEdges);
  b.rawEdges = 0;
  if (!b.armed) { if (now >= BOOT_LOCK_MS) b.armed = true; return; }   // held through boot: just arm
  if (!b.owner) return;
  b.owner = false;
  if (b.longFired) return;                               // LONG already sent, release ignored
  uint32_t held = now - b.downAt;
  if (held >= SHORT_MAX_MS) { cnt[i].midDrops++; return; } // 700..999 ms: neither short nor long

  if (i == B_BACK) {
    if (confirmOpen && now - confirmFirstUp <= DOUBLE_WIN_MS) {
      uint32_t gap = b.downAt - confirmFirstUp;
      if (gap < DOUBLE_GAP_MS) { cnt[i].bounceGaps++; return; }   // too fast = bounce, ignore
      confirmOpen = false; cnt[i].doubles++; pushEv(EV_DOUBLE, B_BACK); return;
    }
    confirmOpen = true; confirmFirstUp = now;
  }
  cnt[i].shorts++; pushEv(EV_SHORT, (BtnId)i);
}

void inputPoll() {
  static uint32_t last = 0;
  uint32_t now = millis();
  if (now - last < POLL_MS) return;
  last = now;
  for (uint8_t i = 0; i < B_COUNT; i++) {
    Btn& b = btn[i];
    bool r = readPressed(i);
    if (r != b.lastRaw) { b.lastRaw = r; b.same = 1; if (b.rawEdges < 999) b.rawEdges++; continue; }
    if (b.same < STABLE_READS) { b.same++; if (b.same < STABLE_READS) continue; }
    if (r != b.stable) {
      b.stable = r;
      if (r) onStablePress(i, now); else onStableRelease(i, now);
    } else if (!r && !b.armed && now >= BOOT_LOCK_MS) {
      b.armed = true;                                    // released & lockout over
    }
    if (b.stable && b.owner && !b.longFired && now - b.downAt >= LONG_MS) {
      b.longFired = true; cnt[i].longs++; pushEv(EV_LONG, (BtnId)i);
    }
  }
  if (confirmOpen && now - confirmFirstUp > DOUBLE_WIN_MS) { confirmOpen = false; pushEv(EV_CONFIRM_TIMEOUT, B_BACK); }
}
