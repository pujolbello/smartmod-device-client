#include "SmartMod.h"

namespace smartmod {

SmartMod::SmartMod() = default;

void SmartMod::use(ICapability& capability) {
  capabilities_.push_back(&capability);
}

String SmartMod::macSuffix() {
  uint64_t chipid = ESP.getEfuseMac();
  uint8_t b[6];
  b[0] = (chipid >> 40) & 0xFF;
  b[1] = (chipid >> 32) & 0xFF;
  b[2] = (chipid >> 24) & 0xFF;
  b[3] = (chipid >> 16) & 0xFF;
  b[4] = (chipid >> 8)  & 0xFF;
  b[5] = chipid         & 0xFF;
  char buf[8];
  // Tomamos los 3 bytes más significativos de la MAC -> 6 hex.
  snprintf(buf, sizeof(buf), "%02X%02X%02X", b[3], b[2], b[1]);
  return String(buf);
}

String SmartMod::sanitizePrefix(const String& raw) {
  String out;
  out.reserve(4);
  for (size_t i = 0; i < raw.length() && out.length() < 4; ++i) {
    char c = raw[i];
    const bool alnum = (c >= '0' && c <= '9') ||
                       (c >= 'a' && c <= 'z') ||
                       (c >= 'A' && c <= 'Z');
    if (alnum) out += c;
  }
  if (out.length() == 0) out = "smd";
  return out;
}

String SmartMod::buildDeviceId(const String& prefix) {
  return sanitizePrefix(prefix) + "-" + macSuffix();
}

String SmartMod::defaultDeviceId() {
  // Compatibilidad histórica: prefijo por defecto "smd".
  return buildDeviceId("smd");
}

void SmartMod::begin() {
  // Cargar configuración persistida (sobreescribe los defaults del sketch).
  store_.load(config_);

  // Sanea el prefijo en cualquier caso (por si el sketch puso algo inválido)
  // y resuelve siempre el deviceId final como "<prefijo>-<MAC6>".
  config_.deviceIdPrefix = sanitizePrefix(config_.deviceIdPrefix);
  config_.deviceId = buildDeviceId(config_.deviceIdPrefix);

  // Si falta configuración mínima o se pidió portal, arrancar provisioning.
  if (forceProvisioning_ || !Store::hasMinimumConfig(config_)) {
    Serial.println("[smartmod] Configuración incompleta, iniciando portal...");
    Portal portal;
    portal.run(config_, store_);   // bloqueante, reinicia al guardar
    return;                        // por seguridad: portal hace ESP.restart()
  }

  topics_.configure(config_.workspace,
                    config_.organization,
                    config_.user,
                    config_.deviceId,
                    config_.protocolVersion);

  transport_.setPayloadCallback(
      [this](const String& topic, const uint8_t* payload, size_t length) {
        handleIncoming(topic, payload, length);
      });
  transport_.setOnConnected([this]() { resubscribe(); });

  for (auto* cap : capabilities_) {
    if (cap) cap->attach(*this);
  }

  transport_.begin(config_, topics_);
}

void SmartMod::factoryReset() {
  store_.clear();
  delay(200);
  ESP.restart();
}

void SmartMod::loop() {
  transport_.loop();
  const bool nowConnected = transport_.connected();
  if (nowConnected != wasConnected_) {
    wasConnected_ = nowConnected;
    if (nowConnected) {
      for (auto* cap : capabilities_) {
        if (cap) cap->onConnected();
      }
    }
  }
  for (auto* cap : capabilities_) {
    if (cap) cap->loop();
  }
}

bool SmartMod::publishOutput(const String& category,
                             const String& format,
                             const String& payload,
                             bool retained) {
  return transport_.publish(topics_.build("output", category, format), payload, retained);
}

void SmartMod::handleIncoming(const String& topic,
                              const uint8_t* payload,
                              size_t length) {
  String category, format;
  if (!Topics::parseCategoryFormat(topic, category, format)) return;
  router_.dispatch(topic, category, format, payload, length);
}

void SmartMod::resubscribe() {
  // PubSubClient pierde las suscripciones tras desconectar.
  transport_.subscribe(topics_.inputWildcard());
}

}  // namespace smartmod
