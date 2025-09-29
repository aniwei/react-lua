#pragma once

#include "HostInstance.h"
#include "HostPropsDiff.h"
#include "jsi/jsi.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace react {

class ReactHostInstance
  : public HostInstance,
    public std::enable_shared_from_this<ReactHostInstance> {
public:
  ReactHostInstance(
    facebook::jsi::Runtime& rt, 
    std::string type, 
    const facebook::jsi::Object& props);
  ReactHostInstance(
    facebook::jsi::Runtime& rt, 
    std::string type, 
    const std::string& textContent);

  void appendChild(std::shared_ptr<HostInstance> child) override;
  void removeChild(std::shared_ptr<HostInstance> child) override;

  void insertChildBefore(
    std::shared_ptr<HostInstance> child,
    std::shared_ptr<HostInstance> beforeChild) override;

  void setAttribute(
    const std::string& key,
    const facebook::jsi::Value& value) override;

  void removeAttribute(const std::string& key) override;
  
  facebook::jsi::Value getAttribute(
    facebook::jsi::Runtime& rt,
    const std::string& key) const override;

  void setTextContent(const std::string& text) override;
  const std::string& getTextContent() const override;

  void applyUpdate(
    const facebook::jsi::Object& newProps,
    const facebook::jsi::Object& payload);

  const std::string& getType() const {
    return type_;
  }

  bool isTextInstance() const {
    return isText_;
  }

  const std::unordered_map<std::string, facebook::jsi::Value>& getProps() const {
    return props_;
  }

  const std::unordered_map<std::string, facebook::jsi::Value>& getEventHandlers() const {
    return eventHandlers_;
  }

private:
  void setProp(
    const std::string& key,
    const facebook::jsi::Value& value);

  void removeProp(const std::string& key);

  facebook::jsi::Runtime* runtime_{nullptr};
  bool isText_{false};
  std::string type_;
  std::string textContent_;
  std::unordered_map<std::string, facebook::jsi::Value> props_;
  std::unordered_map<std::string, facebook::jsi::Value> eventHandlers_;
};

} // namespace react
