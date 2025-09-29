#pragma once

#include "ReactDOMInstance.h"
#include "jsi/jsi.h"
#include <memory>
#include <string>

namespace react {

class ReactDOMHostConfig {
public:
  virtual ~ReactDOMHostConfig() = default;

  virtual std::shared_ptr<ReactDOMInstance> createInstance(
      facebook::jsi::Runtime& rt,
      const std::string& type,
      const facebook::jsi::Object& props) = 0;

  virtual std::shared_ptr<ReactDOMInstance> createTextInstance(
      facebook::jsi::Runtime& rt,
      const std::string& text) = 0;

  virtual void appendChild(
      std::shared_ptr<ReactDOMInstance> parent,
      std::shared_ptr<ReactDOMInstance> child) = 0;
  
  virtual void removeChild(
      std::shared_ptr<ReactDOMInstance> parent,
      std::shared_ptr<ReactDOMInstance> child) = 0;

  virtual void insertBefore(
      std::shared_ptr<ReactDOMInstance> parent,
      std::shared_ptr<ReactDOMInstance> child,
      std::shared_ptr<ReactDOMInstance> beforeChild) = 0;

  virtual void commitUpdate(
      std::shared_ptr<ReactDOMInstance> instance,
      const facebook::jsi::Object& oldProps,
      const facebook::jsi::Object& newProps,
      const facebook::jsi::Object& updatePayload) = 0;
};

} // namespace react
