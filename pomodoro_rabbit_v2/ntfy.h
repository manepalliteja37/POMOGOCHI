// ntfy.h - phone alerts through ntfy.sh (v1.2-b11)
//
// Three alerts, each switchable on the web page:
//   bit0 break over   - a full break finished, the rabbit waits for the next focus
//   bit1 hungry       - Fullness under 20 in free time (8 am - 10 pm), at most once a day
//   bit2 summary      - the day's focuses / minutes / tasks / streak at the summary hour (default 21:00)
// Rules: never sent while a focus or break runs. One pending slot per alert. Plain HTTP POST to
// ntfy.sh:80/<topic> with a raw WiFiClient (no HTTPS, no HTTPClient), 1.5 s timeout, one retry after
// 60 s, then dropped. If the rabbit is asleep (Wi-Fi off) the radio wakes for the send and sleeps again.
// An empty topic = alerts off. The topic is never exported (anyone who knows it can read the alerts).
#pragma once
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "appdata.h"
#include "netsetup.h"
#include "session.h"
#include "gfx.h"

const uint8_t  NT_BREAK = 1, NT_HUNGRY = 2, NT_SUMMARY = 4, NT_TEST = 8;
const uint8_t  NT_HUNGRY_BELOW = 20, NT_HUNGRY_FROM_H = 8, NT_HUNGRY_TO_H = 22;
const uint16_t NT_TIMEOUT_MS = 1500;
const uint32_t NT_RETRY_MS = 60000UL, NT_WAKE_GIVEUP_MS = 60000UL;

uint8_t  ntPending = 0;          // alerts waiting to go out
uint8_t  ntRetried = 0;          // alerts that already failed once
uint32_t ntRetryAt = 0;
bool     ntWokeNet = false;      // we switched Wi-Fi on from sleep just to send
uint32_t ntWokeAt = 0;
char     ntLast[40] = "nothing sent yet";   // shown on the web page

bool ntfyOn() { return settings.ntfyTopic[0] != 0; }
void ntfyQueue(uint8_t bit) {
  if (!ntfyOn()) return;
  if (bit != NT_TEST && !(settings.ntfyFlags & bit)) return;
  ntPending |= bit; ntRetried &= ~bit; ntRetryAt = 0;
}

bool ntfyValidTopic(const String& t) {
  if (!t.length() || t.length() > 32) return false;
  for (uint16_t i = 0; i < t.length(); i++) { char c = t[i]; if (!isalnum(c) && c != '-' && c != '_') return false; }
  return true;
}

// one POST; true when ntfy answered 200
bool ntfyPost(const char* title, const char* tags, const String& body) {
  WiFiClient c;
  c.setTimeout(NT_TIMEOUT_MS);
  uint32_t t0 = millis();
  if (!c.connect("ntfy.sh", 80)) { snprintf(ntLast, sizeof(ntLast), "can't reach ntfy.sh"); return false; }
  String h; h.reserve(220 + body.length());
  h += F("POST /"); h += settings.ntfyTopic; h += F(" HTTP/1.1\r\nHost: ntfy.sh\r\nConnection: close\r\nTitle: ");
  h += title; h += F("\r\nTags: "); h += tags;
  h += F("\r\nContent-Type: text/plain; charset=utf-8\r\nContent-Length: "); h += body.length(); h += F("\r\n\r\n"); h += body;
  c.print(h);
  bool ok = false;
  while (millis() - t0 < NT_TIMEOUT_MS + 500) {
    if (c.available()) { String l = c.readStringUntil('\n'); ok = l.startsWith("HTTP/1.1 200") || l.startsWith("HTTP/1.0 200"); break; }
    if (!c.connected()) break;
    delay(5);
  }
  c.stop();
  Serial.printf("[ntfy] %s -> %s (%lu ms, heap %u)\n", title, ok ? "OK" : "FAILED", (unsigned long)(millis() - t0), ESP.getFreeHeap());
  return ok;
}

bool ntfySend(uint8_t bit) {
  String b; char t[48];
  switch (bit) {
    case NT_BREAK:
      snprintf(t, sizeof(t), "Break over");
      b = String(pet.name) + F(" is waiting for the next focus. Press OK when you're ready.");
      return ntfyPost(t, "rabbit,tomato", b);
    case NT_HUNGRY:
      snprintf(t, sizeof(t), "%s is hungry", pet.name);
      b = String(F("Fullness ")) + pet.fullness + F("%. ");
      b += pet.carrots ? F("Feed a carrot from the care menu.") : F("Finish a focus to earn a carrot.");
      return ntfyPost(t, "rabbit,carrot", b);
    case NT_SUMMARY: {
      snprintf(t, sizeof(t), "%s's day", pet.name);
      uint8_t n = dayLog.n25 + dayLog.n50;
      b = String(F("Focuses: ")) + n + F(" (") + dayLog.focusMin + F(" min)\nTasks done: ") + doneCount();
      b += String(F("\nStreak: ")) + pet.streakCur + (pet.streakCur == 1 ? F(" day") : F(" days"));
      b += n ? F("\nGreat work today!") : F("\nTomorrow is a fresh start.");
      return ntfyPost(t, n ? "rabbit,star" : "rabbit,zzz", b);
    }
    default:
      return ntfyPost("Test from Pomodoro Rabbit", "rabbit,white_check_mark", String(F("Alerts work! ")) + pet.name + F(" says hi."));
  }
}

// once a minute: should the hungry / summary alerts be queued?
void ntfyCheckEvents() {
  if (!ntfyOn() || !clockValid()) return;
  time_t now = time(nullptr); tm lt; localtime_r(&now, &lt);
  uint8_t md = lt.tm_mday;
  if (pet.growth && !sessionActive() && pet.fullness < NT_HUNGRY_BELOW && lt.tm_hour >= NT_HUNGRY_FROM_H &&
      lt.tm_hour < NT_HUNGRY_TO_H && settings.ntHungryMday != md && (settings.ntfyFlags & NT_HUNGRY)) {
    settings.ntHungryMday = md; markDirty(DIRTY_SET); ntfyQueue(NT_HUNGRY);
  }
  if (lt.tm_hour >= settings.summaryHour && settings.ntSummaryMday != md && (settings.ntfyFlags & NT_SUMMARY)) {
    settings.ntSummaryMday = md; markDirty(DIRTY_SET); ntfyQueue(NT_SUMMARY);
  }
}

void ntfyTick() {
  static uint32_t lastChk = 0;
  uint32_t ms = millis();
  if (ms - lastChk >= 60000UL || !lastChk) { lastChk = ms; ntfyCheckEvents(); }
  if (ntWokeNet && scr != SC_SLEEP) ntWokeNet = false;              // someone woke it meanwhile: keep Wi-Fi on
  if (!ntPending) {
    if (ntWokeNet) { ntWokeNet = false; netSleep(); }              // sent: back to sleep
    return;
  }
  if (sessionActive() || portalOn) return;                         // never during focus or break
  if (ntRetryAt && (int32_t)(ms - ntRetryAt) < 0) return;
  if (scr == SC_SLEEP && netAsleep) { netWake(); ntWokeNet = true; ntWokeAt = ms; Serial.println("[ntfy] waking Wi-Fi to send"); return; }
  if (netState != NET_ONLINE) {
    if (ntWokeNet && ms - ntWokeAt > NT_WAKE_GIVEUP_MS) {          // no Wi-Fi: give up, back to sleep
      ntPending = 0; ntWokeNet = false; strlcpy(ntLast, "no Wi-Fi, dropped", sizeof(ntLast));
      if (scr == SC_SLEEP) netSleep();
    }
    return;
  }
  uint8_t bit = 1; while (!(ntPending & bit)) bit <<= 1;             // one alert per loop
  if (ntfySend(bit)) {
    ntPending &= ~bit; ntRetried &= ~bit; ntRetryAt = 0;
    const char* nm = bit == NT_BREAK ? "break over" : bit == NT_HUNGRY ? "hungry" : bit == NT_SUMMARY ? "summary" : "test";
    snprintf(ntLast, sizeof(ntLast), "%s sent at %s", nm, clockHHMM().c_str());
  } else if (ntRetried & bit) {
    ntPending &= ~bit; ntRetried &= ~bit; ntRetryAt = 0;
    strlcat(ntLast, ", dropped", sizeof(ntLast));
  } else { ntRetried |= bit; ntRetryAt = ms + NT_RETRY_MS; strlcat(ntLast, ", retry in 1 min", sizeof(ntLast)); }
}
