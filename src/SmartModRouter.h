#pragma once

#include <Arduino.h>
#include <functional>
#include <vector>

namespace smartmod {

/**
 * Router que despacha mensajes MQTT entrantes a handlers registrados por
 * (category, format). Esto desacopla el transporte de la lógica de aplicación
 * y permite que cada capability registre sus propios endpoints.
 */
class Router {
 public:
  using Handler = std::function<void(const String& topic,
                                     const uint8_t* payload,
                                     size_t length)>;

  void on(const String& category, const String& format, Handler handler);

  // Devuelve true si algún handler atendió el mensaje.
  bool dispatch(const String& topic,
                const String& category,
                const String& format,
                const uint8_t* payload,
                size_t length) const;

 private:
  struct Entry {
    String category;
    String format;
    Handler handler;
  };
  std::vector<Entry> entries_;
};

}  // namespace smartmod
