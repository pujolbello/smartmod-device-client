#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>

#include "ICapability.h"

namespace smartmod {

/**
 * Capability de telemetría / mediciones.
 *
 * Publica payloads en .../output/measurement/json con el formato:
 *   { "fields": [ {"name": value}, ... ], "tags": [ ... ] }
 *
 * El sketch provee una función productora que rellena el JsonDocument; la
 * capability se encarga del temporizado y la publicación.
 */
class MeasurementCapability : public ICapability {
 public:
  // El productor recibe un JsonArray "fields" y opcionalmente "tags" para llenar.
  using Producer = std::function<void(JsonArray fields, JsonArray tags)>;

  const char* name() const override { return "measurement"; }
  void attach(SmartMod& core) override;
  void loop() override;

  void setIntervalMs(uint32_t ms) { intervalMs_ = ms; }
  void setProducer(Producer p) { producer_ = std::move(p); }

  // Publicación inmediata (ignora el temporizador).
  bool publishNow();

 private:
  SmartMod* core_ = nullptr;
  Producer producer_;
  uint32_t intervalMs_ = 1000;
  uint32_t nextPublishMs_ = 0;
};

}  // namespace smartmod
