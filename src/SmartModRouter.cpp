#include "SmartModRouter.h"

namespace smartmod {

void Router::on(const String& category, const String& format, Handler handler) {
  entries_.push_back({category, format, std::move(handler)});
}

bool Router::dispatch(const String& topic,
                      const String& category,
                      const String& format,
                      const uint8_t* payload,
                      size_t length) const {
  bool handled = false;
  for (const auto& e : entries_) {
    if (e.category == category && e.format == format && e.handler) {
      e.handler(topic, payload, length);
      handled = true;
    }
  }
  return handled;
}

}  // namespace smartmod
