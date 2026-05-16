#include "SmartModStore.h"

#include <Preferences.h>

namespace smartmod {

namespace {
constexpr const char* kNs = "smartmod";

String getStr(Preferences& p, const char* k, const String& def) {
  return p.isKey(k) ? p.getString(k, def) : def;
}
}  // namespace

bool Store::load(Config& cfg) const {
  Preferences p;
  if (!p.begin(kNs, /*readOnly*/ true)) return false;

  cfg.wifiSsid     = getStr(p, "wifi_ssid",  cfg.wifiSsid);
  cfg.wifiPassword = getStr(p, "wifi_pass",  cfg.wifiPassword);
  cfg.mqttHost     = getStr(p, "mqtt_host",  cfg.mqttHost);
  cfg.mqttPort     = p.isKey("mqtt_port") ? p.getUShort("mqtt_port", cfg.mqttPort) : cfg.mqttPort;
  cfg.mqttUser     = getStr(p, "mqtt_user",  cfg.mqttUser);
  cfg.mqttPassword = getStr(p, "mqtt_pass",  cfg.mqttPassword);
  cfg.workspace    = getStr(p, "ws",         cfg.workspace);
  cfg.organization = getStr(p, "org",        cfg.organization);
  cfg.user         = getStr(p, "user",       cfg.user);
  cfg.deviceIdPrefix = getStr(p, "dev_pfx",  cfg.deviceIdPrefix);
  cfg.protocolVersion = p.isKey("proto_v")
      ? (uint8_t)p.getUChar("proto_v", cfg.protocolVersion)
      : cfg.protocolVersion;

  const bool hasWifi = cfg.wifiSsid.length() > 0;
  p.end();
  return hasWifi;
}

bool Store::save(const Config& cfg) const {
  Preferences p;
  if (!p.begin(kNs, /*readOnly*/ false)) return false;
  p.putString("wifi_ssid", cfg.wifiSsid);
  p.putString("wifi_pass", cfg.wifiPassword);
  p.putString("mqtt_host", cfg.mqttHost);
  p.putUShort("mqtt_port", cfg.mqttPort);
  p.putString("mqtt_user", cfg.mqttUser);
  p.putString("mqtt_pass", cfg.mqttPassword);
  p.putString("ws",        cfg.workspace);
  p.putString("org",       cfg.organization);
  p.putString("user",      cfg.user);
  p.putString("dev_pfx",   cfg.deviceIdPrefix);
  p.putUChar ("proto_v",   cfg.protocolVersion);
  p.end();
  return true;
}

void Store::clear() const {
  Preferences p;
  if (p.begin(kNs, false)) {
    p.clear();
    p.end();
  }
}

bool Store::hasMinimumConfig(const Config& cfg) {
  return cfg.wifiSsid.length() > 0 &&
         cfg.mqttHost.length() > 0 &&
         cfg.organization.length() > 0 &&
         cfg.user.length() > 0;
}

}  // namespace smartmod
