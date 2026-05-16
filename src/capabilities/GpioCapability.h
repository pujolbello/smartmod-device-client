#pragma once

#include <Arduino.h>
#include <functional>
#include <vector>

#include "ICapability.h"

namespace smartmod {

enum class PinDir : uint8_t { Input, InputPullup, Output };

struct PinConfig {
  uint8_t pin;
  PinDir dir;
  int initialValue = 0;  // solo aplicable a Output
};

/**
 * Capability de GPIO digital.
 *
 *  - Publica cambios de pines de entrada en .../output/gpio/json (con debounce
 *    por intervalo de muestreo configurable).
 *  - Acepta comandos en .../input/gpio/json con el formato
 *    [{"pin":N,"_value":0|1,"mode":"output","type":"digital"}, ...]
 *    que reproduce el contrato actual del backend SmartMod.
 *
 * Mantener la forma JSON ya soportada por el backend permite migrar el
 * firmware existente sin cambios server-side.
 */
class GpioCapability : public ICapability {
 public:
  const char* name() const override { return "gpio"; }
  void attach(SmartMod& core) override;
  void loop() override;
  void onConnected() override;

  void configure(std::initializer_list<PinConfig> pins);
  void configure(const std::vector<PinConfig>& pins);

  using ChangeCallback = std::function<void(uint8_t pin, int value)>;
  void onChange(ChangeCallback cb) { onChange_ = std::move(cb); }

  void setPollIntervalMs(uint32_t ms) { pollIntervalMs_ = ms; }

 private:
  struct PinState {
    PinConfig cfg;
    int lastValue = -1;
  };

  void applyConfig(const PinConfig& cfg);
  void publishPin(const PinState& p);
  void publishSnapshot();
  void handleCommand(const uint8_t* payload, size_t length);

  SmartMod* core_ = nullptr;
  std::vector<PinState> pins_;
  uint32_t pollIntervalMs_ = 100;
  uint32_t nextPollMs_ = 0;
  ChangeCallback onChange_;
};

}  // namespace smartmod
