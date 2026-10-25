// history.h - 9.3b: today's focus log, the day sheet (JSON), 30 days of history, carry-over
//
// During the day   : every finished focus is logged (time, minutes, task, done?) in the "day" save.
// At midnight      : the day sheet is written to /d/YYYYMMDD.json, its summary goes into the "hist"
//                    index (newest first, 30 days kept, older files deleted), tasks done that day leave
//                    the list and unfinished ones carry over (carry +1).
// Web              : GET /api/hist (summary list), GET /api/day?d=YYYYMMDD (download one sheet; today = live)
#pragma once
#include <LittleFS.h>
#include "appdata.h"
#include "clock.h"

void ymdText(char* b, uint32_t ymd) {
  snprintf(b, 11, "%04lu-%02lu-%02lu", (unsigned long)(ymd / 10000), (unsigned long)(ymd / 100 % 100), (unsigned long)(ymd % 100));
}
void sheetPath(char* p, uint32_t ymd) { snprintf(p, 20, "/d/%lu.json", (unsigned long)ymd); }

void jOut(Print& o, const char* s) {
  o.print('"');
  for (; *s; s++) { char c = *s; if (c == '"' || c == '\\') { o.print('\\'); o.print(c); } else if ((uint8_t)c < 32) o.print(' '); else o.print(c); }
  o.print('"');
}

void dayEnsure() {
  uint32_t t = settings.todayDate ? settings.todayDate : clockToday();
  if (!dayLog.ymd && t) { dayLog.ymd = t; markDirty(DIRTY_DAY); }
}

// returns the log row (or -1 when the 12-row log is full; the counters still count)
int8_t logFocus(uint8_t mins, const char* task) {
  dayEnsure();
  if (mins >= 50) dayLog.n50++; else if (mins >= 25) dayLog.n25++; else dayLog.nTest++;
  if (mins >= 25) dayLog.focusMin += mins;
  int8_t r = -1;
  if (dayLog.nLog < LOG_MAX) {
    r = dayLog.nLog++;
    LogE& e = dayLog.log[r]; memset(&e, 0, sizeof(e));
    time_t t = time(nullptr); tm lt; localtime_r(&t, &lt);
    e.minOfDay = clockValid() ? lt.tm_hour * 60 + lt.tm_min : 0xFFFF;
    e.mins = mins;
    if (task && task[0]) strlcpy(e.task, task, sizeof(e.task));
  }
  markDirty(DIRTY_DAY);
  return r;
}

// the day sheet from what is in RAM (today live, or the closing day at midnight)
void writeSheet(Print& o) {
  char d[12]; ymdText(d, dayLog.ymd);
  o.print(F("{\"app\":\"pomodoro\",\"sheet\":1,\"date\":\"")); o.print(d);
  o.printf("\",\"focus\":{\"sessions\":%u,\"minutes\":%u,\"s25\":%u,\"s50\":%u,\"tests\":%u},\"log\":[",
           dayLog.n25 + dayLog.n50, dayLog.focusMin, dayLog.n25, dayLog.n50, dayLog.nTest);
  for (uint8_t i = 0; i < dayLog.nLog; i++) {
    const LogE& e = dayLog.log[i];
    if (i) o.print(',');
    if (e.minOfDay == 0xFFFF) o.print(F("{\"time\":null"));
    else o.printf("{\"time\":\"%02u:%02u\"", e.minOfDay / 60, e.minOfDay % 60);
    o.printf(",\"min\":%u,\"task\":", e.mins);
    if (e.task[0]) jOut(o, e.task); else o.print(F("null"));
    o.print(F(",\"done\":")); o.print(e.done ? F("true}") : F("false}"));
  }
  o.print(F("],\"done\":["));
  bool first = true;
  for (uint8_t i = 0; i < todos.count; i++) if (todos.item[i].done) {
    if (!first) o.print(','); first = false;
    o.print(F("{\"task\":")); jOut(o, todos.item[i].text); o.printf(",\"focus\":%u,\"carried\":%u}", todos.item[i].focus, todos.item[i].carry);
  }
  o.print(F("],\"open\":["));
  first = true;
  for (uint8_t i = 0; i < todos.count; i++) if (!todos.item[i].done) {
    if (!first) o.print(','); first = false;
    o.print(F("{\"task\":")); jOut(o, todos.item[i].text); o.printf(",\"focus\":%u,\"carried\":%u}", todos.item[i].focus, todos.item[i].carry);
  }
  o.printf("],\"pet\":{\"name\":"); jOut(o, pet.name);
  o.printf(",\"fullness\":%u,\"love\":%u,\"energy\":%u,\"growth\":%u,\"carrots\":%u},\"streak\":%u}",
           pet.fullness, pet.love, pet.energy, pet.growth, pet.carrots, pet.streakCur);
}

void historyRollover(uint32_t oldDay, uint32_t newDay) {
  if (!dayLog.ymd) dayLog.ymd = oldDay;
  uint32_t t0 = millis();
  LittleFS.mkdir("/d");
  char p[20]; sheetPath(p, dayLog.ymd);
  File f = LittleFS.open(p, "w");
  bool wrote = (bool)f;
  if (wrote) { writeSheet(f); f.close(); }
  // index: newest first, keep 30, delete the file that falls off
  HistE e = { dayLog.ymd, dayLog.focusMin, (uint8_t)(dayLog.n25 + dayLog.n50), doneCount(), openCount(), 0 };
  uint8_t k = 0;
  while (k < hist.n && hist.e[k].ymd != e.ymd) k++;
  if (k < hist.n) hist.e[k] = e;                               // same day twice (clock jump): replace
  else {
    if (hist.n == HIST_MAX) { char q[20]; sheetPath(q, hist.e[HIST_MAX - 1].ymd); LittleFS.remove(q); hist.n--; }
    memmove(&hist.e[1], &hist.e[0], hist.n * sizeof(HistE));
    hist.e[0] = e; hist.n++;
  }
  // to-dos: done ones leave, the rest carry over
  uint8_t w = 0;
  for (uint8_t i = 0; i < todos.count; i++) if (!todos.item[i].done) {
    todos.item[w] = todos.item[i]; if (todos.item[w].carry < 99) todos.item[w].carry++; w++;
  }
  for (uint8_t i = w; i < TODO_MAX; i++) memset(&todos.item[i], 0, sizeof(TodoItem));
  todos.count = w; todoSel = 0;
  Serial.printf("[history] %lu: %u sessions %u min, %u done, %u carried -> %s (%lu ms)\n", (unsigned long)e.ymd, e.sessions, e.mins,
                e.done, e.open, wrote ? p : "WRITE FAILED", (unsigned long)(millis() - t0));
  dayLog = DayLog(); dayLog.ymd = newDay;
  markDirty(DIRTY_TODO | DIRTY_DAY | DIRTY_HIST);
}
