// clock.h - IST time from NTP, saved epoch fallback (Stage 6b, goes into the final firmware)
//
// Boot: the last saved epoch is loaded at once (time is "saved", behind by however long the
// power was off). When NTP answers, time jumps to the real value ("NTP").
// The epoch is saved every 10 min (in the final firmware: in the heartbeat gap during focus).
// The midnight rollover itself lives in the pet/streak logic: compare clockToday() with the saved day.
#pragma once
#include <Arduino.h>
#include <time.h>
#include <sys/time.h>
#include <coredecls.h>
#include "storage.h"

#define TZ_IST       "IST-5:30"
#define EPOCH_VALID  1700000000UL
#define EPOCH_SAVE_MS (10UL * 60UL * 1000UL)

struct ClockSave { uint32_t epoch; uint8_t everSynced; };
enum ClockSrc : uint8_t { CLK_NONE, CLK_SAVED, CLK_NTP };

ClockSave clockSave = { 0, 0 };
Blob      clockBlob = { "clock", 1, &clockSave, sizeof(clockSave) };
ClockSrc  clockSrc = CLK_NONE;
uint32_t  lastEpochSaveMs = 0, ntpSyncs = 0;
bool      clockFake = false;               // test jumps: never saved
volatile bool ntpJustSynced = false;
uint32_t  manualSetMs = 0;               // our own settimeofday() also fires the callback - ignore it

void onTimeSet() { if (manualSetMs && millis() - manualSetMs < 2000) return; ntpJustSynced = true; }

void clockSet(time_t t) { manualSetMs = millis() | 1; timeval tv = { t, 0 }; settimeofday(&tv, nullptr); }

const char* clockSrcName() { return clockSrc == CLK_NTP ? "NTP" : clockSrc == CLK_SAVED ? "saved epoch" : "none"; }
bool     clockValid() { return clockSrc != CLK_NONE; }
uint32_t clockNow()   { return (uint32_t)time(nullptr); }

uint32_t dayKey(time_t t) { tm lt; localtime_r(&t, &lt); return (lt.tm_year + 1900) * 10000UL + (lt.tm_mon + 1) * 100UL + lt.tm_mday; }
uint32_t clockToday() { return clockValid() ? dayKey(time(nullptr)) : 0; }

int32_t daysBetween(uint32_t a, uint32_t b) {
  tm ta = {}, tb = {};
  ta.tm_year = a / 10000 - 1900; ta.tm_mon = (a / 100) % 100 - 1; ta.tm_mday = a % 100; ta.tm_hour = 12;
  tb.tm_year = b / 10000 - 1900; tb.tm_mon = (b / 100) % 100 - 1; tb.tm_mday = b % 100; tb.tm_hour = 12;
  return (int32_t)((mktime(&tb) - mktime(&ta) + 43200) / 86400);
}

// "23:14" for the top bar, "--:--" if never synced
String clockHHMM() {
  if (!clockValid()) return "--:--";
  time_t t = time(nullptr); tm lt; localtime_r(&t, &lt);
  char b[6]; sprintf(b, "%02d:%02d", lt.tm_hour, lt.tm_min); return b;
}
String clockFull() {
  if (!clockValid()) return "not set";
  time_t t = time(nullptr); tm lt; localtime_r(&t, &lt);
  char b[24]; strftime(b, sizeof(b), "%Y-%m-%d %H:%M:%S", &lt); return b;
}

void clockSaveNow() {
  if (!clockValid() || clockFake) return;
  clockSave.epoch = clockNow(); storeSave(clockBlob); lastEpochSaveMs = millis();
}

void clockBegin() {
  settimeofday_cb(onTimeSet);
  configTime(TZ_IST, "pool.ntp.org", "time.google.com", "time.nist.gov");
  if (storeLoad(clockBlob) && clockSave.epoch > EPOCH_VALID) { clockSet(clockSave.epoch); clockSrc = CLK_SAVED; }
  Serial.printf("[time] boot clock: %s (%s)\n", clockFull().c_str(), clockSrcName());
}

void clockResync() { configTime(TZ_IST, "pool.ntp.org", "time.google.com", "time.nist.gov"); }   // restarts SNTP -> quick sync

void clockTick() {
  if (ntpJustSynced) {
    ntpJustSynced = false; clockFake = false; clockSrc = CLK_NTP; ntpSyncs++;
    clockSave.everSynced = 1; clockSaveNow();
    Serial.printf("[time] NTP sync #%lu: %s IST\n", (unsigned long)ntpSyncs, clockFull().c_str());
  }
  if (clockValid() && millis() - lastEpochSaveMs >= EPOCH_SAVE_MS) clockSaveNow();
}
