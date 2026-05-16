#include "GpioCapability.h"

#include <ArduinoJson.h>

#include "../SmartMod.h"

namespace smartmod {

namespace {
const char* modeStr(PinDir d) {
  switch (d) {
    case PinDir::Output: return "output";
    case PinDir::Input:
    case PinDir::InputPullup:
    default: return "input";
  }
}
}  // namespace

void GpioCapability::attach(SmartMod& core) {
  core_ = &core;
  core.router().on("gpio", "json",
                   [this](const String&, const uint8_t* p, size_t n) {
                     handleCommand(p, n);
                   });
}

void GpioCapability::configure(std::initializer_list<PinConfig> pins) {
  for (const auto& p : pins) applyConfig(p);
}

void GpioCapability::configure(const std::vector<PinConfig>& pins) {
  for (const auto& p : pins) applyConfig(p);
}

void GpioCapability::applyConfig(const PinConfig& cfg) {
  switch (cfg.dir) {
    case PinDir::Output:
      pinMode(cfg.pin, OUTPUT);
      digitalWrite(cfg.pin, cfg.initialValue ? HIGH : LOW);
      break;
    case PinDir::Input:
      pinMode(cfg.pin, INPUT);
      break;
    case PinDir::InputPullup:
      pinMode(cfg.pin, INPUT_PULLUP);
      break;
  }
  PinState st;
  st.cfg = cfg;
  st.lastValue = (cfg.dir == PinDir::Output) ? cfg.initialValue : digitalRead(cfg.pin);
  pins_.push_back(st);
  // No publicar aquí: el transporte aún no está conectado.
  // El snapshot se envía en onConnected() (incluye reconexiones).
}

void GpioCapability::onConnected() {
  // Refrescar lecturas de inputs antes de anunciar el estado.
  for (auto& p : pins_) {
    if (p.cfg.dir != PinDir::Output) {
      p.lastValue = digitalRead(p.cfg.pin);
    }
  }
  publishSnapshot();
}

void GpioCapability::publishSnapshot() {
  if (!core_ || pins_.empty()) return;
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (const auto& p : pins_) {
    JsonObject obj = arr.add<JsonObject>();
    obj["pin"] = p.cfg.pin;
    obj["_value"] = p.lastValue;
    obj["mode"] = modeStr(p.cfg.dir);
    obj["type"] = "digital";
  }
  String payload;
  serializeJson(doc, payload);
  // Retenido para que el backend reciba el estado actual al suscribirse,
  // incluso si no estaba escuchando en el momento del arranque.
  core_->publishOutput("gpio", "json", payload, /*retained*/ true);
}

void GpioCapability::loop() {
  if (!core_) return;
  const uint32_t now = millis();
  if (now < nextPollMs_) return;
  nextPollMs_ = now + pollIntervalMs_;

  for (auto& p : pins_) {
    if (p.cfg.dir == PinDir::Output) continue;
    const int v = digitalRead(p.cfg.pin);
    if (v != p.lastValue) {
      p.lastValue = v;
      publishPin(p);
      if (onChange_) onChange_(p.cfg.pin, v);
    }
  }
}

void GpioCapability::publishPin(const PinState& p) {
  if (!core_) return;
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  JsonObject obj = arr.add<JsonObject>();
  obj["pin"] = p.cfg.pin;
  obj["_value"] = p.lastValue;
  obj["mode"] = modeStr(p.cfg.dir);
  obj["type"] = "digital";

  String payload;
  serializeJson(doc, payload);
  core_->publishOutput("gpio", "json", payload);
}

void GpioCapability::handleCommand(const uint8_t* payload, size_t length) {
  // El backend puede enviar el array directamente o como string JSON escapado.
  String raw;
  raw.reserve(length);
  for (size_t i = 0; i < length; ++i) raw += (char)payload[i];

  if (raw.startsWith("\"[")) {
    raw.replace("\\\"", "\"");
    raw.remove(0, 1);
    if (raw.length() > 0) raw.remove(raw.length() - 1);
  }

  JsonDocument doc;
  if (deserializeJson(doc, raw)) return;
  if (!doc.is<JsonArray>()) return;

  for (JsonObject obj : doc.as<JsonArray>()) {
    const int pin = obj["pin"] | -1;
    const int value = obj["_value"] | 0;
    const char* mode = obj["mode"] | "";
    const char* type = obj["type"] | "";
    if (pin < 0) continue;
    if (strcmp(type, "digital") != 0) continue;
    if (strcmp(mode, "output") != 0) continue;

    // Localiza pin ya configurado para actualizarlo y publicar eco.
    for (auto& p : pins_) {
      if (p.cfg.pin == (uint8_t)pin && p.cfg.dir == PinDir::Output) {
        digitalWrite(p.cfg.pin, value ? HIGH : LOW);
        p.lastValue = value;
        publishPin(p);
        if (onChange_) onChange_(p.cfg.pin, value);
        break;
      }
    }
  }
}

}  // namespace smartmod
