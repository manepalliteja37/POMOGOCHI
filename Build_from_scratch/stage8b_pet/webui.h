// webui.h - the Pomodoro web page + its small API (Stage 7)
//
// GET  /              the page (one PROGMEM file, ~9 KB, no internet/CDN needed)
// GET  /api/state     everything the page shows, as JSON (the page polls it every 2 s while open)
// POST /api/todo      op=add&t= | edit&i=&t= | del&i= | up&i= | down&i= | tick&i=&v= | clear
// POST /api/set       k=mu|v|hb|bl|ba|ob|name &v=
// POST /api/wifi      forget Wi-Fi -> restart into the Pomodoro-Setup hotspot
// GET  /api/export    backup JSON download (Wi-Fi password is NEVER included)
// POST /api/import    the page reads the backup file and posts its fields here
//
// Every POST answers with the new state, so the page redraws at once. The device screen
// redraws on its next loop (well under 1 s). All limits (7 tasks, 32 chars, ASCII) are
// checked here too, not only in the browser.
#pragma once
#include "appdata.h"
#include "netsetup.h"
#include "gfx.h"
#include "session.h"

static const char PAGE_MAIN[] PROGMEM = R"rawliteral(<!doctype html><html><head><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1"><title>Pomodoro</title><style>
body{font-family:system-ui,sans-serif;background:#111;color:#eee;max-width:460px;margin:auto;padding:12px}
h2{margin:6px 0}h3{margin:0 0 8px;font-size:16px;color:#e8833a}
.c{background:#1b1b1b;border-radius:12px;padding:12px;margin:10px 0}
.r{display:flex;align-items:center;gap:6px;padding:6px 0;border-bottom:1px solid #262626}.r:last-child{border:0}
.t{flex:1;word-break:break-all;cursor:pointer}.dn{color:#777;text-decoration:line-through}
button,input,select{font-size:15px;border-radius:8px;border:1px solid #444;background:#222;color:#eee;padding:8px}
button{cursor:pointer}button:disabled{opacity:.3}.p{background:#e8833a;color:#111;border:0;font-weight:bold}
.s{padding:4px 10px}.row{display:flex;gap:6px;flex-wrap:wrap}.row input{flex:1;min-width:0}
small,.m{color:#999}.bar{height:8px;background:#333;border-radius:4px;overflow:hidden;flex:1}.bar i{display:block;height:100%}
.d{display:inline-block;width:13%;text-align:center}.big{font-size:26px;font-weight:bold}
label{display:flex;justify-content:space-between;align-items:center;padding:6px 0;gap:10px}
input[type=checkbox]{width:20px;height:20px}input[type=range]{flex:1;max-width:60%}
</style></head><body>
<h2>&#x1F430; <span id=pn>Pomodoro</span></h2><div class=m id=st>connecting...</div>
<div class=c><h3>To-do <small id=tc></small></h3><div id=tl></div>
<div class=row style="margin-top:8px"><input id=ni maxlength=32 placeholder="New task"><button class=p onclick=add()>Add</button></div>
<div style="margin-top:8px"><button onclick="act('/api/todo',{op:'clear'})">Clear done</button> <small>Tap a task to edit. English only, 32 max.</small></div></div>
<div class=c><h3>Streak</h3><div><span class=big id=sc></span> <span id=sx class=m></span></div>
<div id=s7 style="margin:8px 0"></div><div id=ms class=m></div></div>
<div class=c><h3>Pet</h3><div id=pi></div><div id=pb style="margin-top:6px"></div><button onclick=ren() style="margin-top:8px">Rename</button></div>
<div class=c><h3>Settings</h3>
<label>Mute <input type=checkbox id=mu onchange="set('mu',this.checked?1:0)"></label>
<label>Chime volume <select id=vo onchange="set('v',this.value)"><option value=0>Soft<option value=1>Mid<option value=2>Loud</select></label>
<label>Focus heartbeat <select id=hb onchange="set('hb',this.value)"><option value=0>432 Hz<option value=1>216 Hz deep<option value=2>528 Hz<option value=3>264 Hz deep<option value=4>216 Hz deep, slow</select></label>
<label>Backlight <input type=range id=bl min=30 max=1023 oninput=lv(this.value) onchange="set('bl',this.value)"></label>
<label>Break auto-start <input type=checkbox id=ba onchange="set('ba',this.checked?1:0)"></label>
<div class=row><button onclick="if(confirm('Replay the welcome screens on the device?'))set('ob',0)">Replay onboarding</button><button onclick=wifi()>Change Wi-Fi</button></div>
<div class=m id=wf style="margin-top:6px"></div></div>
<div class=c><h3>Backup</h3><div class=row><button class=p onclick="location='/api/export'">Export JSON</button><button onclick="fi.click()">Import JSON</button></div>
<input type=file id=fi accept=".json,application/json" hidden onchange="imp(this.files[0]);this.value=''">
<small>Keeps to-dos, pet, streak and settings. The Wi-Fi password is never exported.</small></div>
<div class=m id=ft style="text-align:center;font-size:12px"></div>
<script>
const $=i=>document.getElementById(i);let S={rev:-1},busy=0,lt=0;
const E=s=>String(s).replace(/[&<>"']/g,c=>'&#'+c.charCodeAt(0)+';');
function api(p,d){busy++;return fetch(p,{method:'POST',body:new URLSearchParams(d)}).then(x=>x.ok?x.json():x.text().then(t=>{throw t})).then(draw).finally(()=>busy--)}
function act(p,d){return api(p,d).catch(e=>alert(e))}
function set(k,v){return act('/api/set',{k:k,v:v})}
function lv(v){let n=Date.now();if(n-lt>200){lt=n;set('bl',v)}}
function ok(t){t=t.trim();if(!t){return null}if(t.length>32){alert('Max 32 characters');return null}
 if(/[^\x20-\x7e]/.test(t)){alert('English letters, numbers and symbols only. The device font has no Telugu or emoji.');return null}return t}
function add(){let t=ok($('ni').value);if(t===null)return;api('/api/todo',{op:'add',t:t}).then(()=>{$('ni').value=''}).catch(e=>alert(e))}
function ed(i){let t=prompt('Edit task',S.todos[i][0]);if(t===null)return;t=ok(t);if(t!==null)act('/api/todo',{op:'edit',i:i,t:t})}
function op(o,i){if(o=='del'&&!confirm('Delete "'+S.todos[i][0]+'"?'))return;act('/api/todo',{op:o,i:i})}
function tk(i,v){act('/api/todo',{op:'tick',i:i,v:v?1:0})}
function ren(){let t=prompt('Pet name (1-11 characters)',S.pet.name);if(t===null)return;t=t.trim();
 if(!t||t.length>11||/[^\x20-\x7e]/.test(t))return alert('1-11 English characters');set('name',t)}
function wifi(){if(confirm('Forget "'+S.ssid+'" and restart into the setup hotspot Pomodoro-Setup?'))
 api('/api/wifi',{}).then(()=>alert('Restarting. On your phone, join Pomodoro-Setup.')).catch(e=>alert(e))}
function imp(f){if(!f)return;f.text().then(t=>{let d=JSON.parse(t);if(d.app!='pomodoro')throw 'not a Pomodoro backup';
 if(!confirm('Replace the to-dos, pet, streak and settings on the device with this backup?'))return;
 let p={n:d.todos.length};d.todos.forEach((x,i)=>{p['t'+i]=x.t;p['d'+i]=x.d?1:0});
 let q=d.pet,k=d.streak,z=d.settings;Object.assign(p,{name:q.name,g:q.growth,f:q.fullness,l:q.love,e:q.energy,mf:q.moodFlags,sd:q.sickDays,ca:q.carrots||0,
 sc:k.cur,sb:k.best,sh:k.shield,l7:k.last7,ld:k.lastDay,mu:z.muted,v:z.volume,hb:z.heartbeat,bl:z.backlight,ba:z.breakAuto,lp:z.lastPreset});
 return api('/api/import',p).then(()=>alert('Backup restored'))}).catch(e=>alert('Import failed: '+e))}
function bar(n,v){let c=v>60?'#4c4':v>=30?'#e8a33a':'#e44';return '<div class=r style="border:0;padding:3px 0"><span style="width:72px">'+n+
 '</span><div class=bar><i style="width:'+v+'%;background:'+c+'"></i></div><span style="width:28px;text-align:right">'+v+'</span></div>'}
function fmt(u){return u>=3600?Math.floor(u/3600)+'h '+Math.floor(u%3600/60)+'m':Math.floor(u/60)+'m '+u%60+'s'}
function draw(s){
 $('st').textContent=s.time+' IST ('+s.src+')'+(s.ft.run?' \u00b7 session '+s.ft.left+'s left':'');
 $('ft').textContent='Heap '+s.heap+' B \u00b7 up '+fmt(s.up)+' \u00b7 slowest loop '+s.loop+' ms \u00b7 Stage 8b';
 $('wf').textContent='Wi-Fi '+s.ssid+' \u00b7 '+s.rssi+' dBm \u00b7 '+s.ip;
 if(s.rev===S.rev){S=s;return}S=s;let a=document.activeElement;
 $('pn').textContent=s.pet.name;
 let n=s.todos.length,d=s.todos.filter(x=>x[1]).length;$('tc').textContent=n+'/7 \u00b7 '+d+' done';
 $('tl').innerHTML=n?s.todos.map((x,i)=>'<div class=r><input type=checkbox '+(x[1]?'checked ':'')+'onchange="tk('+i+',this.checked)"><span class="t'+
  (x[1]?' dn':'')+'" onclick=ed('+i+')>'+E(x[0])+'</span><button class=s onclick="op(\'up\','+i+')"'+(i?'':' disabled')+
  '>&uarr;</button><button class=s onclick="op(\'down\','+i+')"'+(i<n-1?'':' disabled')+'>&darr;</button><button class=s onclick="op(\'del\','+i+
  ')">&times;</button></div>').join(''):'<small>No tasks yet.</small>';
 $('ni').disabled=n>=7;$('ni').placeholder=n>=7?'List full, 7/7':'New task';
 let k=s.sk;$('sc').textContent='\u{1F525} '+k.c+(k.c==1?' day':' days');
 $('sx').textContent='best '+k.b+(k.sh?' \u00b7 \u{1F6E1} shield ready':'')+' \u00b7 today '+k.td+'/4';
 let w='SMTWTFS',t=new Date().getDay(),h='';for(let i=6;i>=0;i--)h+='<span class=d>'+w[(t-i+7)%7]+'<br>'+((k.l7>>i)&1?'\u2705':'\u274C')+'</span>';
 $('s7').innerHTML=h;$('ms').textContent='Milestones  '+[3,7,30,100].map(m=>(k.b>=m?'\u2714 ':'\u25CB ')+m).join('   ');
 let p=s.pet;$('pi').innerHTML='<b>'+p.st+'</b> \u00b7 Growth '+p.g+(p.nx?' / '+p.nx+' for next stage':' (fully grown)')+' \u00b7 '+p.mood+' \u00b7 \u{1F955} '+p.ca;
 $('pb').innerHTML=bar('Fullness',p.f)+bar('Love',p.l)+bar('Energy',p.e);
 let z=s.set;if(a!=$('mu'))$('mu').checked=!!z.mu;if(a!=$('vo'))$('vo').value=z.v;if(a!=$('hb'))$('hb').value=z.hb;
 if(a!=$('bl'))$('bl').value=z.bl;if(a!=$('ba'))$('ba').checked=!!z.ba}
function poll(){if(!document.hidden&&!busy)fetch('/api/state').then(x=>x.json()).then(draw).catch(()=>{})}
$('ni').onkeydown=e=>{if(e.key=='Enter')add()};setInterval(poll,2000);poll();
</script></body></html>)rawliteral";

// ---------- helpers ----------
void jStr(String& j, const char* s) {
  j += '"';
  for (; *s; s++) { char c = *s; if (c == '"' || c == '\\') { j += '\\'; j += c; } else if ((uint8_t)c < 32) j += ' '; else j += c; }
  j += '"';
}

void bad(const String& msg) { server.send(400, "text/plain", msg); }

// trims, then checks 1..maxLen printable ASCII characters
bool cleanText(String& t, uint8_t maxLen, String& err) {
  t.trim();
  if (!t.length()) { err = "Text is empty"; return false; }
  if (t.length() > maxLen) { err = "Max " + String(maxLen) + " characters"; return false; }
  for (size_t i = 0; i < t.length(); i++) {
    uint8_t c = t[i];
    if (c < 32 || c > 126) { err = "English letters, numbers and symbols only"; return false; }
  }
  return true;
}

long argNum(const char* k, long lo, long hi, long def) {
  if (!server.hasArg(k)) return def;
  long v = server.arg(k).toInt();
  return v < lo ? lo : v > hi ? hi : v;
}

// ---------- state JSON ----------
void sendState() {
  String j; j.reserve(1500);
  j += F("{\"rev\":"); j += rev;
  j += F(",\"time\":"); jStr(j, clockFull().c_str());
  j += F(",\"src\":"); jStr(j, clockSrcName());
  j += F(",\"net\":"); jStr(j, netStateName());
  j += F(",\"ssid\":"); jStr(j, wifiCfg.ssid);
  j += F(",\"ip\":"); jStr(j, WiFi.localIP().toString().c_str());
  j += F(",\"rssi\":"); j += WiFi.RSSI();
  j += F(",\"heap\":"); j += ESP.getFreeHeap();
  j += F(",\"up\":"); j += (uint32_t)(millis() / 1000);
  j += F(",\"loop\":"); j += (uint32_t)(loopMaxUs / 1000);
  j += F(",\"todos\":[");
  for (uint8_t i = 0; i < todos.count; i++) {
    if (i) j += ',';
    j += '['; jStr(j, todos.item[i].text); j += ','; j += (todos.item[i].done ? 1 : 0); j += ']';
  }
  uint8_t st = stageOf(pet.growth);
  j += F("],\"pet\":{\"name\":"); jStr(j, pet.name);
  j += F(",\"g\":"); j += pet.growth;
  j += F(",\"st\":"); jStr(j, STAGE_NAME[st]);
  j += F(",\"nx\":"); j += nextStageAt(st);
  j += F(",\"f\":"); j += pet.fullness;
  j += F(",\"l\":"); j += pet.love;
  j += F(",\"e\":"); j += pet.energy;
  j += F(",\"mood\":"); jStr(j, moodName());
  j += F(",\"ca\":"); j += pet.carrots;
  j += F("},\"sk\":{\"c\":"); j += pet.streakCur;
  j += F(",\"b\":"); j += pet.streakBest;
  j += F(",\"sh\":"); j += pet.shield;
  j += F(",\"td\":"); j += settings.todayCount;
  j += F(",\"l7\":"); j += pet.last7;
  j += F("},\"set\":{\"mu\":"); j += (muted ? 1 : 0);
  j += F(",\"v\":"); j += volume;
  j += F(",\"hb\":"); j += hbSel;
  j += F(",\"bl\":"); j += settings.backlight;
  j += F(",\"ba\":"); j += settings.breakAuto;
  j += F(",\"ob\":"); j += settings.onboarded;
  j += F("},\"ft\":{\"run\":"); j += (sessionActive() ? 1 : 0);
  j += F(",\"left\":"); j += sessionLeftSec();
  j += F("}}");
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", j);
}

// ---------- handlers ----------
void handlePage() { server.send_P(200, "text/html", PAGE_MAIN); }
void handleState() { sendState(); }

void handleTodo() {
  String op = server.arg("op");
  int i = server.hasArg("i") ? server.arg("i").toInt() : -1;
  bool needI = !(op == "add" || op == "clear");
  if (needI && (i < 0 || i >= todos.count)) return bad("That task no longer exists - page refreshed");
  String t, err;
  if (op == "add" || op == "edit") { t = server.arg("t"); if (!cleanText(t, 32, err)) return bad(err); }

  if (op == "add") {
    if (todos.count >= 7) return bad("List full, 7/7");
    TodoItem& it = todos.item[todos.count];
    memset(&it, 0, sizeof(it)); strlcpy(it.text, t.c_str(), sizeof(it.text));
    todos.count++;
  } else if (op == "edit") {
    memset(todos.item[i].text, 0, sizeof(todos.item[i].text)); strlcpy(todos.item[i].text, t.c_str(), sizeof(todos.item[i].text));
  } else if (op == "del") {
    for (uint8_t k = i; k + 1 < todos.count; k++) todos.item[k] = todos.item[k + 1];
    todos.count--; memset(&todos.item[todos.count], 0, sizeof(TodoItem));
  } else if (op == "up" || op == "down") {
    int k = (op == "up") ? i - 1 : i + 1;
    if (k < 0 || k >= todos.count) return sendState();
    TodoItem tmp = todos.item[i]; todos.item[i] = todos.item[k]; todos.item[k] = tmp;
    if (todoSel == i) todoSel = k; else if (todoSel == k) todoSel = i;
  } else if (op == "tick") {
    todos.item[i].done = server.hasArg("v") ? (server.arg("v").toInt() ? 1 : 0) : !todos.item[i].done;
  } else if (op == "clear") {
    uint8_t w = 0;
    for (uint8_t k = 0; k < todos.count; k++) if (!todos.item[k].done) todos.item[w++] = todos.item[k];
    for (uint8_t k = w; k < 7; k++) memset(&todos.item[k], 0, sizeof(TodoItem));
    todos.count = w; todoSel = 0;
  } else return bad("Unknown action");

  Serial.printf("[web] todo %s -> %u/7\n", op.c_str(), todos.count);
  markDirty(DIRTY_TODO);
  sendState();
}

void handleSet() {
  String k = server.arg("k");
  long v = server.arg("v").toInt();
  if (k == "mu") setMuted(v != 0);
  else if (k == "v") {
    settings.volume = volume = constrain(v, 0, 2);
    if (!sessionActive()) playChime(1);                         // preview at the new volume (never during a session)
    markDirty(DIRTY_SET);
  } else if (k == "hb") {
    settings.hbSel = hbSel = constrain(v, 0, N_HB - 1);
    if (hbWanted) { if (!bellPending && !muted) startHeartbeat(); }   // switch the running heartbeat
    else if (!muted && !sessionActive()) { buildHeartbeat(); playMel(hbNotes, false, HB_DUTY); }   // preview one beat
    markDirty(DIRTY_SET);
  } else if (k == "bl") { settings.backlight = constrain(v, 30, 1023); applyBacklight(); markDirty(DIRTY_SET); }
  else if (k == "ba") { settings.breakAuto = v ? 1 : 0; markDirty(DIRTY_SET); }
  else if (k == "ob") { settings.onboarded = 0; onboardRequest = true; markDirty(DIRTY_SET); Serial.println("[web] replay onboarding"); }
  else if (k == "name") {
    String t = server.arg("v"), err;
    if (!cleanText(t, 11, err)) return bad(err);
    memset(pet.name, 0, sizeof(pet.name)); strlcpy(pet.name, t.c_str(), sizeof(pet.name));
    markDirty(DIRTY_PET);
  } else return bad("Unknown setting");
  Serial.printf("[web] set %s\n", k.c_str());
  sendState();
}

void handleWifiReset() {
  sendState();
  Serial.println("[web] Change Wi-Fi -> forgetting creds, restarting into setup portal");
  storeRemove(wifiBlob); memset(&wifiCfg, 0, sizeof(wifiCfg));
  restartPending = true; restartAt = millis() + 1500;           // netTick() restarts after the reply is sent
}

void handleExport() {
  String j; j.reserve(1200);
  j += F("{\"app\":\"pomodoro\",\"ver\":1,\"exported\":"); jStr(j, clockFull().c_str());
  j += F(",\"todos\":[");
  for (uint8_t i = 0; i < todos.count; i++) {
    if (i) j += ',';
    j += F("{\"t\":"); jStr(j, todos.item[i].text); j += F(",\"d\":"); j += (todos.item[i].done ? 1 : 0); j += '}';
  }
  j += F("],\"pet\":{\"name\":"); jStr(j, pet.name);
  j += F(",\"growth\":"); j += pet.growth;
  j += F(",\"fullness\":"); j += pet.fullness;
  j += F(",\"love\":"); j += pet.love;
  j += F(",\"energy\":"); j += pet.energy;
  j += F(",\"moodFlags\":"); j += pet.moodFlags;
  j += F(",\"sickDays\":"); j += pet.sickDays;
  j += F(",\"carrots\":"); j += pet.carrots;
  j += F("},\"streak\":{\"cur\":"); j += pet.streakCur;
  j += F(",\"best\":"); j += pet.streakBest;
  j += F(",\"shield\":"); j += pet.shield;
  j += F(",\"last7\":"); j += pet.last7;
  j += F(",\"lastDay\":"); j += pet.lastStreakDay;
  j += F("},\"settings\":{\"muted\":"); j += settings.muted;
  j += F(",\"volume\":"); j += settings.volume;
  j += F(",\"heartbeat\":"); j += settings.hbSel;
  j += F(",\"backlight\":"); j += settings.backlight;
  j += F(",\"breakAuto\":"); j += settings.breakAuto;
  j += F(",\"lastPreset\":"); j += settings.lastPreset;
  j += F("}}");                                                    // no Wi-Fi block on purpose
  char fn[64]; snprintf(fn, sizeof(fn), "attachment; filename=\"pomodoro-backup-%lu.json\"", (unsigned long)clockToday());
  server.sendHeader("Content-Disposition", fn);
  server.send(200, "application/json", j);
  Serial.printf("[web] export %u bytes\n", j.length());
}

void handleImport() {
  long n = argNum("n", -1, 99, -1);
  if (n < 0) return bad("Backup has no to-do list");
  if (n > 7) return bad("Backup has more than 7 tasks");
  // check every task before touching anything
  String txt[7], err;
  for (uint8_t i = 0; i < n; i++) {
    char k[4]; snprintf(k, sizeof(k), "t%u", i);
    txt[i] = server.arg(k);
    if (!cleanText(txt[i], 32, err)) return bad("Task " + String(i + 1) + ": " + err);
  }
  String nm = server.arg("name");
  if (!cleanText(nm, 11, err)) nm = "Bunny";

  memset(&todos, 0, sizeof(todos));
  for (uint8_t i = 0; i < n; i++) {
    char k[4]; snprintf(k, sizeof(k), "d%u", i);
    strlcpy(todos.item[i].text, txt[i].c_str(), sizeof(todos.item[i].text));
    todos.item[i].done = argNum(k, 0, 1, 0);
  }
  todos.count = n; todoSel = 0;

  memset(pet.name, 0, sizeof(pet.name)); strlcpy(pet.name, nm.c_str(), sizeof(pet.name));
  pet.growth     = argNum("g", 0, 60000, pet.growth);
  pet.stage      = stageOf(pet.growth);
  pet.fullness   = argNum("f", 0, 100, pet.fullness);
  pet.love       = argNum("l", 0, 100, pet.love);
  pet.energy     = argNum("e", 0, 100, pet.energy);
  pet.moodFlags  = argNum("mf", 0, 3, pet.moodFlags);
  pet.sickDays   = argNum("sd", 0, 255, pet.sickDays);
  pet.carrots    = argNum("ca", 0, 9, pet.carrots);
  pet.streakCur  = argNum("sc", 0, 60000, pet.streakCur);
  pet.streakBest = argNum("sb", 0, 60000, pet.streakBest);
  if (pet.streakBest < pet.streakCur) pet.streakBest = pet.streakCur;
  pet.shield     = argNum("sh", 0, 1, pet.shield);
  pet.last7      = argNum("l7", 0, 127, pet.last7);
  pet.lastStreakDay = argNum("ld", 0, 29991231L, pet.lastStreakDay);

  settings.muted      = argNum("mu", 0, 1, settings.muted);
  settings.volume     = argNum("v", 0, 2, settings.volume);
  settings.hbSel      = argNum("hb", 0, N_HB - 1, settings.hbSel);
  settings.backlight  = argNum("bl", 30, 1023, settings.backlight);
  settings.breakAuto  = argNum("ba", 0, 1, settings.breakAuto);
  settings.lastPreset = argNum("lp", 0, 3, settings.lastPreset);
  applySettings();
  if (muted) stopSound(); else if (hbWanted && !bellPending) startHeartbeat();

  Serial.printf("[web] import: %u tasks, %s growth %u, streak %u\n", todos.count, pet.name, pet.growth, pet.streakCur);
  markDirty(DIRTY_SET | DIRTY_PET | DIRTY_TODO);
  sendState();
}

void webBegin() {
  netHomePage = handlePage;                 // netsetup.h serves "/" with this when not in the setup portal
  server.on("/api/state",  HTTP_GET,  handleState);
  server.on("/api/todo",   HTTP_POST, handleTodo);
  server.on("/api/set",    HTTP_POST, handleSet);
  server.on("/api/wifi",   HTTP_POST, handleWifiReset);
  server.on("/api/export", HTTP_GET,  handleExport);
  server.on("/api/import", HTTP_POST, handleImport);
}
