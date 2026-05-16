#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <functional>

#include "SmartModConfig.h"
#include "SmartModTopics.h"

namespace smartmod {

/**
 * Wrapper no bloqueante sobre WiFi + PubSubClient.
 *
 *  - Máquina de estados WiFi / MQTT (no usa while-blocking ni delay()).
 *  - Backoff exponencial entre intentos de conexión.
 *  - Buffer MQTT configurable (PubSubClient por defecto = 256 B, insuficiente
 *    para los payloads de medición de SmartMod).
 *  - Last Will Testament publicado en .../output/status/json para que el
 *    backend detecte desconexiones inesperadas.
 */
class Transport {
 public:
  enum class State { Idle, WifiConnecting, MqttConnecting, Connected };

  using PayloadCallback = std::function<void(const String& topic,
                                             const uint8_t* payload,
                                             size_t length)>;
  using ConnectedCallback = std::function<void()>;

  Transport();

  void begin(const Config& config, const Topics& topics);
  void loop();

  bool connected() const;
  State state() const { return state_; }

  bool publish(const String& topic, const String& payload, bool retained = false);
  bool publish(const String& topic, const uint8_t* payload, size_t length,
               bool retained = false);
  bool subscribe(const String& topic, uint8_t qos = 0);

  void setPayloadCallback(PayloadCallback cb) { payloadCb_ = std::move(cb); }
  void setOnConnected(ConnectedCallback cb) { onConnected_ = std::move(cb); }

 private:
  void handleWifi();
  void handleMqtt();
  void onMqttMessage(char* topic, uint8_t* payload, unsigned int length);

  Config config_{};
  const Topics* topics_ = nullptr;

  WiFiClient netClient_;
  PubSubClient mqtt_;

  State state_ = State::Idle;
  uint32_t nextAttemptMs_ = 0;
  uint32_t backoffMs_ = 0;

  // Estado del intento WiFi en curso.
  bool wifiAttemptInFlight_ = false;
  uint32_t lastWifiBeginMs_ = 0;
  static constexpr uint32_t kWifiAttemptTimeoutMs = 15000;

  PayloadCallback payloadCb_;
  ConnectedCallback onConnected_;

  // Trampoline para PubSubClient (no admite std::function directamente).
  static Transport* instance_;
};

}  // namespace smartmod
