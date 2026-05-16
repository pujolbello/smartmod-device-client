#pragma once

#include <Arduino.h>

namespace smartmod {

class SmartMod;  // forward

/**
 * Interfaz que implementa cada capability (GPIO, Measurement, OTA, ...).
 * El núcleo SmartMod invoca attach()/loop() y ofrece acceso a Topics/Router/Transport.
 */
class ICapability {
 public:
  virtual ~ICapability() = default;
  virtual const char* name() const = 0;
  virtual void attach(SmartMod& core) = 0;
  virtual void loop() {}
  // Invocado por SmartMod en cada transición a estado conectado (incluye reconexiones).
  // Pensado para que las capabilities publiquen su estado inicial / snapshot retenido.
  virtual void onConnected() {}
};

}  // namespace smartmod
