#pragma once

#include <Arduino.h>
#include <vector>

#include "SmartModConfig.h"
#include "SmartModPortal.h"
#include "SmartModRouter.h"
#include "SmartModStore.h"
#include "SmartModTopics.h"
#include "SmartModTransport.h"
#include "capabilities/ICapability.h"

namespace smartmod {

/**
 * Fachada principal de la librería SmartMod.
 *
 * Compone los bloques (Config, Topics, Transport, Router) y orquesta las
 * capabilities registradas por el sketch.
 *
 * Uso típico:
 *   SmartMod mod;
 *   GpioCapability gpio;
 *   MeasurementCapability meas;
 *
 *   void setup() {
 *     auto& cfg = mod.config();
 *     cfg.wifiSsid = "...";
 *     cfg.wifiPassword = "...";
 *     cfg.mqttHost = "api.smartmod.app";
 *     cfg.organization = "smartmod";
 *     cfg.user = "pujolbello";
 *     mod.use(gpio);
 *     mod.use(meas);
 *     mod.begin();
 *   }
 *   void loop() { mod.loop(); }
 */
class SmartMod {
 public:
  SmartMod();

  Config& config() { return config_; }
  const Config& config() const { return config_; }

  Topics& topics() { return topics_; }
  Router& router() { return router_; }
  Transport& transport() { return transport_; }

  void use(ICapability& capability);

  // Fuerza el portal de provisioning en el próximo begin() (independiente de
  // si hay configuración persistida). Útil para botones de "setup".
  void requestProvisioning() { forceProvisioning_ = true; }

  // Borra la configuración persistida y reinicia.
  void factoryReset();

  void begin();
  void loop();

  bool connected() const { return transport_.connected(); }

  // Helpers de publicación que reutilizan el builder de tópicos.
  bool publishOutput(const String& category,
                     const String& format,
                     const String& payload,
                     bool retained = false);

  // Genera deviceId desde la MAC si no se configuró uno explícito.
  static String defaultDeviceId();

  // Sufijo derivado de la MAC (últimos 6 hex, mayúsculas).
  static String macSuffix();

  // Sanea el prefijo: mantiene solo [A-Za-z0-9], trunca a 4 chars,
  // fallback a "smd" si queda vacío.
  static String sanitizePrefix(const String& raw);

  // Compone el clientId/deviceId final "<prefijo>-<MAC6>".
  static String buildDeviceId(const String& prefix);

 private:
  void handleIncoming(const String& topic, const uint8_t* payload, size_t length);
  void resubscribe();

  Config config_;
  Topics topics_;
  Router router_;
  Transport transport_;
  Store store_;
  std::vector<ICapability*> capabilities_;
  bool wasConnected_ = false;
  bool forceProvisioning_ = false;
};

}  // namespace smartmod
