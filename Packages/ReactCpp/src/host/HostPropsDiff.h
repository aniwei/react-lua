#pragma once

#include "jsi/jsi.h"
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace react {

struct HostPropsDiffResult {
  std::unordered_map<std::string, facebook::jsi::Value> attributesToSet;
  std::vector<std::string> attributesToRemove;
  std::unordered_map<std::string, facebook::jsi::Value> eventsToSet;
  std::vector<std::string> eventsToRemove;
  std::optional<std::string> textContent;

  bool hasChanges() const;
};

bool isEventPropKey(const std::string& key);

HostPropsDiffResult diffHostProps(
  facebook::jsi::Runtime& rt,
  const facebook::jsi::Value& previousProps,
  const facebook::jsi::Value& nextProps);

} // namespace react
