// netsetup.h - Wi-Fi join + own captive setup portal (Stage 6b, goes into the final firmware)
//
// No saved Wi-Fi (or can't join it within 30 s at boot) -> hotspot "Pomodoro-Setup" opens.
// Phone joins it -> setup page pops up (captive DNS) -> pick Wi-Fi, type password -> saved
// through storage.h -> restart -> joins your Wi-Fi.
// Everything is NON-BLOCKING: call netTick() every loop(), the timer/pet keep running.
// Wi-Fi lost later -> auto-reconnect quietly (no hotspot, just the crossed Wi-Fi icon).
// The same ESP8266WebServer 'server' is reused by the Stage 7 page.
#pragma once
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include "storage.h"

struct WifiCfg { char ssid[33]; char pass[65]; };
enum NetState : uint8_t { NET_OFF, NET_CONNECTING, NET_ONLINE, NET_LOST, NET_PORTAL };

WifiCfg  wifiCfg = {};
Blob     wifiBlob = { "wifi", 1, &wifiCfg, sizeof(wifiCfg) };
NetState netState = NET_OFF;
ESP8266WebServer server(80);
DNSServer dns;
const char* AP_NAME = "Pomodoro-Setup";
IPAddress apIP(192, 168, 4, 1);
bool     portalOn = false, restartPending = false;
uint32_t netStateMs = 0, restartAt = 0, lastRetryMs = 0;
uint8_t  lastSta = 0;
void   (*netHomePage)() = nullptr;     // the sketch's own "/" page when online (Stage 7 page later)

const char* netStateName() {
  switch (netState) { case NET_CONNECTING: return "connecting"; case NET_ONLINE: return "online";
                      case NET_LOST: return "lost (reconnecting)"; case NET_PORTAL: return "setup portal"; default: return "off"; }
}
void netSetState(NetState s) { netState = s; netStateMs = millis(); }

static const char PAGE_HEAD[] PROGMEM =
  "<!doctype html><html><head><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
  "<title>Pomodoro</title><style>body{font-family:sans-serif;background:#111;color:#eee;max-width:420px;margin:auto;padding:16px}"
  "input,button{width:100%;padding:12px;margin:6px 0;font-size:16px;box-sizing:border-box;border-radius:8px;border:1px solid #444;background:#222;color:#eee}"
  "button{background:#e8833a;border:0;color:#111;font-weight:bold}.n{padding:10px;margin:4px 0;background:#1d1d1d;border-radius:8px;cursor:pointer}"
  "small{color:#999}</style></head><body><h2>&#x1F430; Pomodoro</h2>";

String htmlEsc(const String& s) {
  String o; o.reserve(s.length() + 8);
  for (char c : s) { if (c == '<') o += "&lt;"; else if (c == '>') o += "&gt;"; else if (c == '&') o += "&amp;";
                     else if (c == '"') o += "&quot;"; else if (c == '\'') o += "&#39;"; else o += c; }
  return o;
}

void handleSetupPage() {
  String h = FPSTR(PAGE_HEAD);
  h += F("<p>Tap your Wi-Fi, type the password, save.</p>");
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_FAILED) WiFi.scanNetworks(true);          // start an async scan
  if (n > 0) for (int i = 0; i < n && i < 15; i++) {
    String e = htmlEsc(WiFi.SSID(i)); if (!e.length()) continue;
    h += "<div class=n data-s=\""; h += e; h += "\">"; h += e; h += " <small>"; h += WiFi.RSSI(i); h += " dBm</small></div>";
  } else h += F("<small>Scanning... reload in 5 s for the list, or type the name.</small>");
  h += F("<form method=post action=/save id=f><input name=s placeholder='Wi-Fi name' maxlength=32 required>"
         "<input name=p type=password placeholder='Password' maxlength=64><button>Save &amp; connect</button></form>"
         "<script>document.querySelectorAll('.n').forEach(e=>e.onclick=()=>{f.s.value=e.dataset.s;f.p.focus()})</script></body></html>");
  server.send(200, "text/html", h);
}

void handleSave() {
  String s = server.arg("s"), p = server.arg("p");
  if (!s.length() || s.length() > 32 || p.length() > 64) { server.send(400, "text/plain", "Name 1-32 chars, password max 64"); return; }
  memset(&wifiCfg, 0, sizeof(wifiCfg));
  s.toCharArray(wifiCfg.ssid, sizeof(wifiCfg.ssid));
  p.toCharArray(wifiCfg.pass, sizeof(wifiCfg.pass));
  bool ok = storeSave(wifiBlob);
  String h = FPSTR(PAGE_HEAD);
  h += ok ? "<p>Saved <b>" + htmlEsc(s) + "</b>.</p><p>Pomodoro restarts and joins it. You can close this page.</p>"
          : String("<p>Save FAILED. Try again.</p>");
  h += F("</body></html>");
  server.send(200, "text/html", h);
  Serial.printf("[net] saved Wi-Fi '%s' -> restarting\n", wifiCfg.ssid);
  if (ok) { restartPending = true; restartAt = millis() + 1500; }
}

void handleRoot() {
  if (portalOn) handleSetupPage();
  else if (netHomePage) netHomePage();
  else server.send(200, "text/plain", "Pomodoro");
}

void handleNotFound() {
  if (portalOn) { server.sendHeader("Location", "http://192.168.4.1/", true); server.send(302, "text/plain", ""); }  // captive pop-up
  else server.send(404, "text/plain", "Not found");
}

void startPortal() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(AP_NAME);
  dns.setErrorReplyCode(DNSReplyCode::NoError);
  dns.start(53, "*", apIP);
  WiFi.scanNetworks(true);
  portalOn = true; lastRetryMs = millis(); lastSta = 0;
  netSetState(NET_PORTAL);
  Serial.printf("[net] SETUP PORTAL ON: on your phone join Wi-Fi '%s' (no password), page opens itself or go to http://192.168.4.1\n", AP_NAME);
}

void stopPortal() {
  dns.stop(); WiFi.softAPdisconnect(true); WiFi.mode(WIFI_STA); portalOn = false;
  Serial.println("[net] setup portal OFF");
}

void netBegin(bool offline) {
  WiFi.persistent(false);            // creds live in storage.h, the SDK flash area is never written
  WiFi.setAutoConnect(false);
  storeLoad(wifiBlob);
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.onNotFound(handleNotFound);
  server.begin();
  if (offline) { WiFi.mode(WIFI_OFF); netSetState(NET_OFF); Serial.println("[net] OFFLINE TEST: Wi-Fi off"); return; }
  if (wifiCfg.ssid[0]) {
    WiFi.mode(WIFI_STA); WiFi.setAutoReconnect(true);
    WiFi.begin(wifiCfg.ssid, wifiCfg.pass);
    netSetState(NET_CONNECTING);
    Serial.printf("[net] connecting to '%s'...\n", wifiCfg.ssid);
  } else startPortal();
}

void netForget() {
  storeRemove(wifiBlob); memset(&wifiCfg, 0, sizeof(wifiCfg));
  Serial.println("[net] Wi-Fi forgotten -> restarting into setup portal");
  delay(100); ESP.restart();
}

void netTick() {
  server.handleClient();
  if (portalOn) dns.processNextRequest();
  if (restartPending && (int32_t)(millis() - restartAt) >= 0) ESP.restart();
  wl_status_t st = WiFi.status();
  switch (netState) {
    case NET_CONNECTING:
      if (st == WL_CONNECTED) { netSetState(NET_ONLINE); Serial.printf("[net] ONLINE  ip %s  rssi %d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI()); }
      else if (millis() - netStateMs > 30000) { Serial.println("[net] can't join saved Wi-Fi in 30 s"); startPortal(); }
      break;
    case NET_ONLINE:
      if (st != WL_CONNECTED) { netSetState(NET_LOST); Serial.println("[net] Wi-Fi LOST - auto-reconnecting"); }
      break;
    case NET_LOST:
      if (st == WL_CONNECTED) { netSetState(NET_ONLINE); Serial.printf("[net] back ONLINE  ip %s\n", WiFi.localIP().toString().c_str()); }
      break;
    case NET_PORTAL: {
      uint8_t sta = WiFi.softAPgetStationNum();
      if (wifiCfg.ssid[0]) {
        if (st == WL_CONNECTED) { stopPortal(); netSetState(NET_ONLINE); Serial.printf("[net] ONLINE  ip %s\n", WiFi.localIP().toString().c_str()); break; }
        if (sta > 0 && lastSta == 0) WiFi.disconnect();          // phone joined: stop retries so the hotspot stays stable
        if (sta == 0 && millis() - lastRetryMs > 120000) {       // nobody setting up: retry the saved Wi-Fi every 2 min
          lastRetryMs = millis(); WiFi.begin(wifiCfg.ssid, wifiCfg.pass);
          Serial.println("[net] portal idle - retrying saved Wi-Fi");
        }
      }
      lastSta = sta;
    } break;
    default: break;
  }
}
