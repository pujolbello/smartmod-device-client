#pragma once

#include <Arduino.h>

namespace smartmod {

/**
 * Construye y parsea tópicos siguiendo la convención SmartMod:
 *
 *   workspace/{org}/{user}/{deviceId}/v{N}/{direction}/{category}/{format}
 *
 *   direction = "input"  -> mensajes hacia el dispositivo
 *   direction = "output" -> mensajes desde el dispositivo
 *
 * Centralizar aquí el contrato permite versionar (v1, v2, ...) sin tocar
 * el resto de la librería ni los sketches.
 */
class Topics {
 public:
  Topics() = default;

  void configure(const String& workspace,
                 const String& organization,
                 const String& user,
                 const String& deviceId,
                 uint8_t protocolVersion);

  // Prefijo completo hasta {deviceId}/v{N}
  String base() const;

  // Construye .../{direction}/{category}/{format}
  String build(const String& direction,
               const String& category,
               const String& format) const;

  // Wildcard de suscripción a todas las entradas: .../input/#
  String inputWildcard() const;

  // Extrae (category, format) del final del tópico recibido.
  // Devuelve false si el tópico no respeta la estructura mínima.
  static bool parseCategoryFormat(const String& topic,
                                  String& outCategory,
                                  String& outFormat);

 private:
  String workspace_;
  String organization_;
  String user_;
  String deviceId_;
  uint8_t protocolVersion_ = 1;
};

}  // namespace smartmod
