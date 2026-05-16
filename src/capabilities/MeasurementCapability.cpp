#include "MeasurementCapability.h"

#include <ArduinoJson.h>

#include "../SmartMod.h"

namespace smartmod {

void MeasurementCapability::attach(SmartMod& core) { core_ = &core; }

void MeasurementCapability::loop() {
  if (!core_ || !producer_) return;
  if (!core_->connected()) return;
  const uint32_t now = millis();
  if (now < nextPublishMs_) return;
  nextPublishMs_ = now + intervalMs_;
  publishNow();
}

bool MeasurementCapability::publishNow() {
  if (!core_ || !producer_) return false;

  JsonDocument doc;
  JsonArray fields = doc["fields"].to<JsonArray>();
  JsonArray tags = doc["tags"].to<JsonArray>();
  producer_(fields, tags);

  String payload;
  serializeJson(doc, payload);
  return core_->publishOutput("measurement", "json", payload);
}

}  // namespace smartmod
