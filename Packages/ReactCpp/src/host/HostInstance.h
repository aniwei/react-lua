#pragma once

#include "jsi/jsi.h"
#include <string>
#include <vector>
#include <memory>

namespace react {

class HostInstance : public facebook::jsi::HostObject {
public:
  // DOM-like API
  virtual void appendChild(std::shared_ptr<HostInstance> child) = 0;
  virtual void removeChild(std::shared_ptr<HostInstance> child) = 0;
  
  // jsi::HostObject interface
  facebook::jsi::Value get(facebook::jsi::Runtime&, const facebook::jsi::PropNameID& name) override;
  void set(facebook::jsi::Runtime&, const facebook::jsi::PropNameID& name, const facebook::jsi::Value& value) override;
  std::vector<facebook::jsi::PropNameID> getPropertyNames(facebook::jsi::Runtime& rt) override;

  // Public properties
  std::string tagName;
  std::string className;

  // Parent/Child relationships
  std::weak_ptr<HostInstance> parent;
  std::vector<std::shared_ptr<HostInstance>> children;

protected:
  // For host-specific internal data or handles
  void* hostData_ = nullptr; 
};

} // namespace react
