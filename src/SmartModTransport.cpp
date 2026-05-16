#include "SmartModTransport.h"

namespace smartmod {

Transport* Transport::instance_ = nullptr;

Transport::Transport() : mqtt_(netClient_) {}

void Transport::begin(const Config& config, const Topics& topics) {
  config_ = config;
  topics_ = &topics;
  instance_ = this;

  mqtt_.setServer(config_.mqttHost.c_str(), config_.mqttPort);
  mqtt_.setKeepAlive(config_.keepAliveSeconds);
  mqtt_.setBufferSize(config_.mqttBufferSize);
  mqtt_.setCallback([](char* topic, uint8_t* payload, unsigned int length) {
    if (instance_) instance_->onMqttMessage(topic, payload, length);
  });

  WiFi.mode(WIFI_STA);
  state_ = State::WifiConnecting;
  nextAttemptMs_ = 0;
  backoffMs_ = config_.reconnectMinMs;
}

void Transport::loop() {
  handleWifi();
  if (WiFi.status() == WL_CONNECTED) {
    handleMqtt();
    if (mqtt_.connected()) mqtt_.loop();
  }
}

bool Transport::connected() const {
  return state_ == State::Connected;
}

void Transport::handleWifi() {
  const wl_status_t status = WiFi.status();
  if (status == WL_CONNECTED) {
    if (state_ == State::WifiConnecting) state_ = State::MqttConnecting;
    wifiAttemptInFlight_ = false;
    return;
  }
  state_ = State::WifiConnecting;
  const uint32_t now = millis();

  // Mientras un intento siga en curso (IDLE / DISCONNECTED tras begin()),
  // esperamos hasta el timeout configurado antes de relanzar begin().
  if (wifiAttemptInFlight_) {
    const bool timedOut = (now - lastWifiBeginMs_) >= kWifiAttemptTimeoutMs;
    const bool hardFail = (status == WL_CONNECT_FAILED ||
                           status == WL_NO_SSID_AVAIL ||
                           status == WL_CONNECTION_LOST);
    if (!timedOut && !hardFail) return;
    wifiAttemptInFlight_ = false;
    nextAttemptMs_ = now + backoffMs_;
    backoffMs_ = min<uint32_t>(backoffMs_ * 2, config_.reconnectMaxMs);
    return;
  }

  if (now < nextAttemptMs_) return;

  WiFi.disconnect(true, false);
  WiFi.begin(config_.wifiSsid.c_str(), config_.wifiPassword.c_str());
  lastWifiBeginMs_ = now;
  wifiAttemptInFlight_ = true;
}

void Transport::handleMqtt() {
  if (mqtt_.connected()) {
    if (state_ != State::Connected) {
      state_ = State::Connected;
      backoffMs_ = config_.reconnectMinMs;
      if (onConnected_) onConnected_();
    }
    return;
  }

  state_ = State::MqttConnecting;
  const uint32_t now = millis();
  if (now < nextAttemptMs_) return;

  const String clientId = config_.deviceId;
  const String willTopic = topics_ ? topics_->build("output", "status", "json") : String();
  const char* willPayload = "{\"online\":false}";

  bool ok = false;
  if (willTopic.length() > 0) {
    ok = mqtt_.connect(clientId.c_str(),
                       config_.mqttUser.c_str(),
                       config_.mqttPassword.c_str(),
                       willTopic.c_str(),
                       /*willQos*/ 0,
                       /*willRetain*/ true,
                       willPayload);
  } else {
    ok = mqtt_.connect(clientId.c_str(),
                       config_.mqttUser.c_str(),
                       config_.mqttPassword.c_str());
  }

  if (ok) {
    state_ = State::Connected;
    backoffMs_ = config_.reconnectMinMs;
    if (topics_) {
      // Publica estado online retenido (contrapartida al LWT).
      mqtt_.publish(willTopic.c_str(), "{\"online\":true}", true);
    }
    if (onConnected_) onConnected_();
  } else {
    nextAttemptMs_ = now + backoffMs_;
    backoffMs_ = min<uint32_t>(backoffMs_ * 2, config_.reconnectMaxMs);
  }
}

bool Transport::publish(const String& topic, const String& payload, bool retained) {
  if (!mqtt_.connected()) return false;
  return mqtt_.publish(topic.c_str(),
                       reinterpret_cast<const uint8_t*>(payload.c_str()),
                       payload.length(),
                       retained);
}

bool Transport::publish(const String& topic, const uint8_t* payload, size_t length,
                        bool retained) {
  if (!mqtt_.connected()) return false;
  return mqtt_.publish(topic.c_str(), payload, length, retained);
}

bool Transport::subscribe(const String& topic, uint8_t qos) {
  if (!mqtt_.connected()) return false;
  return mqtt_.subscribe(topic.c_str(), qos);
}

void Transport::onMqttMessage(char* topic, uint8_t* payload, unsigned int length) {
  if (payloadCb_) payloadCb_(String(topic), payload, length);
}

}  // namespace smartmod
