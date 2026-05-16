/**
 * SmartMod — Ejemplo básico
 *
 * Inicializa la fachada SmartMod con dos capabilities (GPIO + Measurement),
 * conecta a WiFi/MQTT y abre el portal cautivo de provisioning si no hay
 * configuración persistida o si el botón BOOT (GPIO0) está pulsado durante
 * el arranque.
 *
 * Hardware:
 *   - Cualquier ESP32 (probado en DOIT DevKit V1)
 *   - GPIO4 / GPIO23 con pull-up interno como entradas
 *   - GPIO2 (LED on-board) como salida
 *
 * Primer arranque:
 *   1. El dispositivo crea el AP "SmartMod-XXXXXX".
 *   2. Conéctate desde tu móvil y completa el formulario.
 *   3. Tras guardar, el módulo reinicia y se conecta a tu MQTT.
 *
 * Forzar el portal después: mantén pulsado BOOT mientras se enciende.
 */

#include <Arduino.h>

#include <SmartMod.h>
#include <capabilities/GpioCapability.h>
#include <capabilities/MeasurementCapability.h>

using namespace smartmod;

SmartMod mod;
GpioCapability gpio;
MeasurementCapability meas;

constexpr uint8_t kProvisionButtonPin = 0;  // BOOT en el DevKit

void setup() {
  Serial.begin(115200);
  Serial.println("\nSmartMod basic example");

  // Defaults compilados: solo se usan si NVS está vacío. El portal cautivo
  // los sobreescribe cuando el usuario guarda el formulario.
  auto& cfg = mod.config();
  cfg.mqttHost        = "api.smartmod.app";
  cfg.mqttPort        = 1883;
  cfg.organization    = "tu-org";
  cfg.user            = "tu-usuario";
  cfg.protocolVersion = 1;

  // Forzar portal con BOOT pulsado al arrancar
  pinMode(kProvisionButtonPin, INPUT_PULLUP);
  delay(50);
  if (digitalRead(kProvisionButtonPin) == LOW) {
    Serial.println("[boot] BOOT pulsado -> portal de provisioning");
    mod.requestProvisioning();
  }

  // Capabilities
  mod.use(gpio);
  mod.use(meas);

  gpio.configure({
      PinConfig{4,  PinDir::InputPullup},
      PinConfig{23, PinDir::InputPullup},
      PinConfig{2,  PinDir::Output, 1},
  });
  gpio.onChange([](uint8_t pin, int value) {
    Serial.printf("[gpio] pin %u -> %d\n", pin, value);
  });

  meas.setIntervalMs(1000);
  meas.setProducer([](JsonArray fields, JsonArray /*tags*/) {
    JsonObject t = fields.add<JsonObject>();
    t["temp"] = random(170, 260) / 10.0f;

    JsonObject v = fields.add<JsonObject>();
    v["voltage"] = random(1050, 1210) / 10.0f;
  });

  mod.begin();
}

void loop() {
  mod.loop();
}
