#pragma once

#include <Arduino.h>

namespace smartmod {

/**
 * Configuración del cliente SmartMod.
 *
 * En esta fase los valores se asignan en runtime desde el sketch.
 * En fases posteriores se persistirán en NVS (Preferences) y se podrán
 * provisionar mediante portal cautivo (WiFiManager propio).
 */
struct Config {
  // WiFi
  String wifiSsid;
  String wifiPassword;

  // Broker MQTT
  String mqttHost;
  uint16_t mqttPort = 1883;
  String mqttUser;
  String mqttPassword;

  // Identidad lógica dentro de la plataforma SmartMod
  String workspace = "workspace";   // raíz del namespace
  String organization;              // p.ej. "tecnoconciencia"
  String user;                      // p.ej. "pujolbello"
  uint8_t protocolVersion = 1;      // v1, v2, ...

  // Prefijo del Device ID. El identificador final es siempre
  //   "<prefijo>-<últimos 6 hex de la MAC>"
  // para garantizar unicidad como clientId MQTT. Solo se admiten
  // hasta 4 caracteres alfanuméricos; valores inválidos se sanean
  // (caracteres no permitidos se eliminan; vacío -> "smd").
  String deviceIdPrefix = "smd";

  // Device ID resuelto en runtime por SmartMod::begin(). No editar
  // manualmente: cualquier valor previo será sobrescrito.
  String deviceId;

  // Tamaño del buffer MQTT (PubSubClient por defecto 256 B, insuficiente para
  // payloads de medición). Ajustable por sketch.
  uint16_t mqttBufferSize = 8192;

  // Keepalive y reintentos
  uint16_t keepAliveSeconds = 30;
  uint32_t reconnectMinMs = 1000;
  uint32_t reconnectMaxMs = 30000;
};

}  // namespace smartmod
