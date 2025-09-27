#pragma once

#include "HostInstance.h"
#include "jsi/jsi.h"
#include <memory>

namespace react {

// HostConfig defines how the Reconciler interacts with the host environment (e.g., DOM, custom UI)
class HostConfig {
public:
  virtual ~HostConfig() = default;

  virtual std::shared_ptr<HostInstance> createInstance(
      facebook::jsi::Runtime& rt,
      const std::string& type,
      const facebook::jsi::Object& props) = 0;

  virtual std::shared_ptr<HostInstance> createTextInstance(
      facebook::jsi::Runtime& rt,
      const std::string& text) = 0;

  virtual void appendChild(
      std::shared_ptr<HostInstance> parent,
      std::shared_ptr<HostInstance> child) = 0;
  
  virtual void removeChild(
      std::shared_ptr<HostInstance> parent,
      std::shared_ptr<HostInstance> child) = 0;

  virtual void insertBefore(
      std::shared_ptr<HostInstance> parent,
      std::shared_ptr<HostInstance> child,
      std::shared_ptr<HostInstance> beforeChild) = 0;

  virtual void commitUpdate(
      std::shared_ptr<HostInstance> instance,
      const facebook::jsi::Object& oldProps,
      const facebook::jsi::Object& newProps,
      const facebook::jsi::Object& updatePayload) = 0;

  // ... other HostConfig methods like getPublicInstance, prepareUpdate, etc.
};

} // namespace react
