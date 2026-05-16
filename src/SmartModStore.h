#pragma once

#include <Arduino.h>

#include "SmartModConfig.h"

namespace smartmod {

/**
 * Persistencia de la configuración en NVS (Preferences).
 *
 * Se almacena bajo el namespace "smartmod". Cada campo se guarda como clave
 * individual para permitir migraciones incrementales sin invalidar todo el
 * blob almacenado.
 */
class Store {
 public:
  // Carga campos existentes en NVS *sobreescribiendo* los del Config.
  // Devuelve true si se cargó al menos la red WiFi.
  bool load(Config& cfg) const;

  // Persiste la configuración completa.
  bool save(const Config& cfg) const;

  // Borra todo el namespace (factory reset).
  void clear() const;

  // ¿Hay configuración mínima válida (WiFi + broker)?
  static bool hasMinimumConfig(const Config& cfg);
};

}  // namespace smartmod
