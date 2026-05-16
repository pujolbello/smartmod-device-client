#include "SmartModPortal.h"

#include <DNSServer.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <algorithm>

#include "SmartMod.h"
#include "SmartModStore.h"
#include "assets/logo_svg.h"

namespace smartmod {

namespace {
constexpr uint8_t kDnsPort = 53;
constexpr uint16_t kHttpPort = 80;

const char* kCss =
    "body{font-family:system-ui,sans-serif;background:#0e1117;color:#e6edf3;"
    "max-width:520px;margin:0 auto;padding:24px}"
    "h1{font-weight:600;font-size:1.4rem;margin-bottom:4px}"
    "p.sub{color:#8b949e;margin-top:0}"
    "fieldset{border:1px solid #30363d;border-radius:8px;padding:12px 16px;margin:16px 0}"
    "legend{padding:0 6px;color:#58a6ff}"
    "label{display:block;font-size:.85rem;color:#8b949e;margin-top:8px}"
    "input{width:100%;box-sizing:border-box;background:#161b22;border:1px solid #30363d;"
    "color:#e6edf3;border-radius:6px;padding:8px;font-size:1rem}"
    "select{width:100%;box-sizing:border-box;background:#161b22;border:1px solid #30363d;"
    "color:#e6edf3;border-radius:6px;padding:8px;font-size:1rem}"
    "input[readonly]{opacity:.6}"
    "button{margin-top:16px;width:100%;background:#238636;border:0;color:#fff;"
    "padding:12px;border-radius:6px;font-size:1rem;cursor:pointer}"
    "button:hover{background:#2ea043}"
    ".ok{color:#3fb950}"
    ".err{color:#f85149}"
    ".spin{width:48px;height:48px;border:4px solid #30363d;border-top-color:#58a6ff;"
    "border-radius:50%;margin:24px auto;animation:r 1s linear infinite}"
    "@keyframes r{to{transform:rotate(360deg)}}";
}  // namespace

String Portal::escape(const String& s) {
  String out;
  out.reserve(s.length());
  for (size_t i = 0; i < s.length(); ++i) {
    char c = s[i];
    switch (c) {
      case '&':  out += "&amp;"; break;
      case '<':  out += "&lt;"; break;
      case '>':  out += "&gt;"; break;
      case '"':  out += "&quot;"; break;
      case '\'': out += "&#39;"; break;
      default:   out += c;
    }
  }
  return out;
}

String Portal::apSsid() const {
  String id = SmartMod::defaultDeviceId();   // "ESP32-AABBCCDDEEFF"
  String suffix = id.substring(id.length() - 6);
  return "SmartMod-" + suffix;
}

String Portal::htmlForm(const Config& cfg, const std::vector<WifiAp>& aps) {
  String h;
  h.reserve(2560);
  h += "<!doctype html><html><head><meta charset='utf-8'>";
  h += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  h += "<title>SmartMod Provisioning</title><style>";
  h += kCss;
  h += "</style></head><body>";
  h += "<div style='text-align:center;margin-bottom:8px'>"
       "<img src='/logo.svg' alt='SmartMod' width='72' height='82'></div>";
  h += "<h1>SmartMod</h1><p class='sub'>Configura el dispositivo para conectarlo a la plataforma.</p>";
  h += "<form method='POST' action='/save'>";

  h += "<fieldset><legend>WiFi</legend>";
  h += "<label>Red</label>";
  h += "<select name='wifi_ssid' id='ssidSel' "
       "onchange=\"document.getElementById('ssidManual').style.display="
       "(this.value===''?'block':'none')\">";
  bool currentInList = false;
  for (const auto& ap : aps) {
    const bool sel = (ap.ssid == cfg.wifiSsid);
    if (sel) currentInList = true;
    h += "<option value='" + escape(ap.ssid) + "'";
    if (sel) h += " selected";
    h += ">" + escape(ap.ssid);
    h += ap.open ? " (abierta)" : " \xF0\x9F\x94\x92";  // candado UTF-8
    h += " " + String(ap.rssi) + "dBm</option>";
  }
  h += "<option value=''";
  if (!currentInList) h += " selected";
  h += ">— Otra red —</option>";
  h += "</select>";
  h += "<input id='ssidManual' name='wifi_ssid_manual' placeholder='SSID manual' value='";
  if (!currentInList) h += escape(cfg.wifiSsid);
  h += "' style='margin-top:6px;display:";
  h += currentInList ? "none" : "block";
  h += "'>";
  h += "<label>Contraseña</label><input name='wifi_pass' type='password' value='" + escape(cfg.wifiPassword) + "'>";
  h += "</fieldset>";

  h += "<fieldset><legend>Broker MQTT</legend>";
  h += "<label>Host</label><input name='mqtt_host' value='" + escape(cfg.mqttHost) +
       (cfg.mqttHost.length() ? "' readonly>" : "' required>");
  h += "<label>Puerto</label><input name='mqtt_port' type='number' value='" + String(cfg.mqttPort) + "' readonly>";
  h += "<label>Usuario</label><input name='user' value='" + escape(cfg.user) + "' required>";
  h += "<label>Contraseña</label><input name='mqtt_pass' type='password' value='" + escape(cfg.mqttPassword) + "'>";
  h += "</fieldset>";

  h += "<fieldset><legend>Workspace Configuration</legend>";
  if (cfg.organization.length()) {
    h += "<label>Workspace</label><input name='org' value='" + escape(cfg.organization) + "' readonly>";
  } else {
    h += "<label>Workspace</label><input name='org' value='' required>";
  }
  h += "</fieldset>";

  h += "<button type='submit'>Guardar y reiniciar</button>";
  h += "</form></body></html>";
  return h;
}

std::vector<Portal::WifiAp> Portal::scanNetworks() {
  std::vector<WifiAp> out;
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, false);
  delay(100);
  const int n = WiFi.scanNetworks(/*async*/ false, /*show_hidden*/ false);
  if (n <= 0) return out;
  out.reserve(n);
  for (int i = 0; i < n; ++i) {
    WifiAp ap;
    ap.ssid = WiFi.SSID(i);
    ap.rssi = WiFi.RSSI(i);
    ap.open = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
    if (ap.ssid.length() == 0) continue;
    out.push_back(ap);
  }
  // Ordenar por RSSI descendente y deduplicar SSIDs (más fuerte primero).
  std::sort(out.begin(), out.end(),
            [](const WifiAp& a, const WifiAp& b) { return a.rssi > b.rssi; });
  std::vector<WifiAp> dedup;
  dedup.reserve(out.size());
  for (const auto& ap : out) {
    bool dup = false;
    for (const auto& d : dedup) {
      if (d.ssid == ap.ssid) { dup = true; break; }
    }
    if (!dup) dedup.push_back(ap);
  }
  WiFi.scanDelete();
  return dedup;
}

void Portal::run(Config& cfg, Store& store) {
  Serial.println("[portal] escaneando redes WiFi...");
  std::vector<WifiAp> aps = scanNetworks();
  Serial.printf("[portal] %u redes detectadas\n", (unsigned)aps.size());

  const String ssid = apSsid();
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid.c_str());
  delay(100);
  const IPAddress apIp = WiFi.softAPIP();

  DNSServer dns;
  dns.start(kDnsPort, "*", apIp);

  WebServer http(kHttpPort);

  // Portal cautivo: cualquier ruta sirve el formulario.
  auto serveForm = [&]() {
    http.send(200, "text/html", htmlForm(cfg, aps));
  };
  http.on("/", HTTP_GET, serveForm);
  http.on("/generate_204", HTTP_GET, serveForm);   // Android
  http.on("/hotspot-detect.html", HTTP_GET, serveForm);  // iOS / macOS
  http.on("/logo.svg", HTTP_GET, [&]() {
    http.sendHeader("Cache-Control", "public, max-age=31536000, immutable");
    http.send_P(200, "image/svg+xml", kLogoSvg, kLogoSvgLen);
  });
  http.onNotFound(serveForm);

  // Estado del intento de conexión tras /save (compartido entre /save y /status).
  // Fases: 0=idle, 1=wifi, 2=mqtt, 3=ok, 4=fail
  static uint8_t connectPhase = 0;
  static uint32_t connectDeadline = 0;
  constexpr uint32_t kWifiTimeoutMs = 20000;
  constexpr uint32_t kMqttTimeoutMs = 10000;
  static WiFiClient mqttNet;
  static PubSubClient mqttProbe(mqttNet);

  http.on("/save", HTTP_POST, [&]() {
    // Si el usuario escribió un SSID manual, tiene prioridad sobre el select.
    const String manual = http.arg("wifi_ssid_manual");
    cfg.wifiSsid     = manual.length() ? manual : http.arg("wifi_ssid");
    cfg.wifiPassword = http.arg("wifi_pass");
    cfg.mqttHost     = http.arg("mqtt_host");
    cfg.mqttPort     = (uint16_t)http.arg("mqtt_port").toInt();
    cfg.mqttPassword = http.arg("mqtt_pass");
    cfg.organization = http.arg("org");
    cfg.user         = http.arg("user");
    // SmartMod usa el mismo username para identidad lógica y autenticación MQTT.
    cfg.mqttUser     = cfg.user;

    const bool saved = store.save(cfg);
    if (!saved) {
      http.send(500, "text/html",
                "<html><body style='font-family:sans-serif;padding:24px'>"
                "<h2 class='err'>Error guardando configuración</h2></body></html>");
      return;
    }

    // Lanzar intento de conexión sin tumbar el AP (modo AP+STA).
    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(cfg.wifiSsid.c_str(), cfg.wifiPassword.c_str());
    connectDeadline = millis() + kWifiTimeoutMs;
    connectPhase = 1;

    String body = "<!doctype html><html><head><meta charset='utf-8'>";
    body += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
    body += "<title>Conectando…</title><style>";
    body += kCss;
    body += "</style></head><body><h1 id='title'>Conectando a WiFi…</h1>";
    body += "<p class='sub' id='sub'>Verificando credenciales de <b>" + escape(cfg.wifiSsid) + "</b></p>";
    body += "<div class='spin'></div>";
    body += "<p id='msg' style='text-align:center;color:#8b949e'>Esto puede tardar hasta 30 segundos…</p>";
    body += "<script>"
            "const T=document.getElementById('title'),S=document.getElementById('sub');"
            "async function poll(){"
            " try{const r=await fetch('/status',{cache:'no-store'});const t=(await r.text()).trim();"
            "  if(t==='wifi'){T.textContent='Conectando a WiFi…';}"
            "  else if(t==='mqtt'){T.textContent='Conectando al broker MQTT…';S.textContent='Autenticando contra el broker';}"
            "  else if(t==='ok'){document.body.innerHTML=\"<h1 class='ok'>Conectado</h1>"
            "<p>WiFi y broker MQTT verificados. El dispositivo se reiniciará en 2 segundos.</p>\";"
            "setTimeout(()=>fetch('/reboot'),1500);return;}"
            "  else if(t.indexOf('fail')===0){"
            "   const reason=t==='fail_wifi'?'No se pudo conectar al WiFi. Revisa el SSID y la contraseña.'"
            "   :t==='fail_mqtt'?'WiFi OK, pero el broker MQTT rechazó la conexión. Revisa host, usuario y contraseña.'"
            "   :'No se pudo establecer la conexión.';"
            "   document.body.innerHTML=\"<h1 class='err'>Error</h1><p>\"+reason+\"</p>"
            "<p><a href='/' style='color:#58a6ff'>Volver al formulario</a></p>\";return;}"
            " }catch(e){}"
            " setTimeout(poll,1000);"
            "}poll();"
            "</script></body></html>";
    http.send(200, "text/html", body);
  });

  http.on("/status", HTTP_GET, [&]() {
    const char* state = "connecting";
    switch (connectPhase) {
      case 0:
        state = "connecting";
        break;
      case 1:  // WiFi
        if (WiFi.status() == WL_CONNECTED) {
          // Pasar a fase MQTT
          mqttProbe.setServer(cfg.mqttHost.c_str(), cfg.mqttPort);
          mqttProbe.setSocketTimeout(5);
          const String clientId = SmartMod::buildDeviceId(
              SmartMod::sanitizePrefix(cfg.deviceIdPrefix));
          if (cfg.mqttUser.length()) {
            mqttProbe.connect(clientId.c_str(), cfg.mqttUser.c_str(), cfg.mqttPassword.c_str());
          } else {
            mqttProbe.connect(clientId.c_str());
          }
          connectPhase = 2;
          connectDeadline = millis() + kMqttTimeoutMs;
          state = "mqtt";
        } else if ((int32_t)(millis() - connectDeadline) >= 0) {
          state = "fail_wifi";
          WiFi.disconnect(true, false);
          WiFi.mode(WIFI_AP);
          connectPhase = 4;
        } else {
          state = "wifi";
        }
        break;
      case 2:  // MQTT
        if (mqttProbe.connected()) {
          state = "ok";
          mqttProbe.disconnect();
          connectPhase = 3;
        } else if ((int32_t)(millis() - connectDeadline) >= 0) {
          state = "fail_mqtt";
          WiFi.disconnect(true, false);
          WiFi.mode(WIFI_AP);
          connectPhase = 4;
        } else {
          // Reintento ligero: PubSubClient.connect es bloqueante; un solo intento basta.
          // Volvemos a probar si todavía no expiró el deadline.
          const String clientId = SmartMod::buildDeviceId(
              SmartMod::sanitizePrefix(cfg.deviceIdPrefix));
          if (cfg.mqttUser.length()) {
            mqttProbe.connect(clientId.c_str(), cfg.mqttUser.c_str(), cfg.mqttPassword.c_str());
          } else {
            mqttProbe.connect(clientId.c_str());
          }
          state = mqttProbe.connected() ? "ok" : "mqtt";
          if (mqttProbe.connected()) { mqttProbe.disconnect(); connectPhase = 3; }
        }
        break;
      case 3:
        state = "ok";
        break;
      case 4:
      default:
        // El cliente JS interpreta cualquier prefijo "fail" como error final.
        // Reusamos el último motivo guardado en el flujo previo.
        state = "fail";
        break;
    }
    http.send(200, "text/plain", state);
  });

  http.on("/reboot", HTTP_GET, [&]() {
    http.send(200, "text/plain", "ok");
    delay(500);
    ESP.restart();
  });

  http.begin();

  Serial.print("[portal] AP listo: ");
  Serial.print(ssid);
  Serial.print(" -> http://");
  Serial.println(apIp);

  while (true) {
    dns.processNextRequest();
    http.handleClient();
    delay(1);
  }
}

}  // namespace smartmod
