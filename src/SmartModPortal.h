#pragma once

#include <Arduino.h>
#include <vector>

#include "SmartModConfig.h"

namespace smartmod {

class Store;

/**
 * Portal cautivo de provisioning.
 *
 * Levanta un AP "SmartMod-XXXX", un DNSServer que redirige *.* hacia la IP
 * del AP y un WebServer con formulario en /. Al recibir POST /save, persiste
 * los campos en NVS mediante Store y reinicia el dispositivo.
 *
 * Es bloqueante por diseño: si llegamos al portal es porque no hay forma
 * de conectar a internet, así que dedicar el CPU al portal es correcto.
 */
class Portal {
 public:
  struct WifiAp {
    String ssid;
    int32_t rssi;
    bool open;
  };

  // Inicia el portal y se queda en bucle hasta que el usuario guarda y
  // confirma (entonces se ejecuta ESP.restart()).
  void run(Config& cfg, Store& store);

 private:
  static String htmlForm(const Config& cfg, const std::vector<WifiAp>& aps);
  static String escape(const String& s);
  static std::vector<WifiAp> scanNetworks();
  String apSsid() const;
};

}  // namespace smartmod
