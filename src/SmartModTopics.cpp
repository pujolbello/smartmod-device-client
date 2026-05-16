#include "SmartModTopics.h"

namespace smartmod {

void Topics::configure(const String& workspace,
                       const String& organization,
                       const String& user,
                       const String& deviceId,
                       uint8_t protocolVersion) {
  workspace_ = workspace;
  organization_ = organization;
  user_ = user;
  deviceId_ = deviceId;
  protocolVersion_ = protocolVersion;
}

String Topics::base() const {
  String b;
  b.reserve(workspace_.length() + organization_.length() + user_.length() +
            deviceId_.length() + 8);
  b += workspace_;
  b += '/';
  b += organization_;
  b += '/';
  b += user_;
  b += '/';
  b += deviceId_;
  b += "/v";
  b += String(protocolVersion_);
  return b;
}

String Topics::build(const String& direction,
                     const String& category,
                     const String& format) const {
  String t = base();
  t += '/';
  t += direction;
  t += '/';
  t += category;
  t += '/';
  t += format;
  return t;
}

String Topics::inputWildcard() const {
  String t = base();
  t += "/input/#";
  return t;
}

bool Topics::parseCategoryFormat(const String& topic,
                                 String& outCategory,
                                 String& outFormat) {
  int lastSlash = topic.lastIndexOf('/');
  if (lastSlash <= 0) return false;
  int secondLastSlash = topic.lastIndexOf('/', lastSlash - 1);
  if (secondLastSlash < 0) return false;
  outCategory = topic.substring(secondLastSlash + 1, lastSlash);
  outFormat = topic.substring(lastSlash + 1);
  return outCategory.length() > 0 && outFormat.length() > 0;
}

}  // namespace smartmod
