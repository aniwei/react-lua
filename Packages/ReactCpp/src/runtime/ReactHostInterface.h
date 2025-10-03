#pragma once

#include <memory>
#include <string>

namespace facebook {
namespace jsi {
class Runtime;
class Object;
} // namespace jsi
} // namespace facebook

namespace react {

class ReactDOMInstance;

class HostInterface {
public:
  virtual ~HostInterface() = default;

  virtual std::shared_ptr<ReactDOMInstance> createHostInstance(
      facebook::jsi::Runtime& runtime,
      const std::string& type,
      const facebook::jsi::Object& props);

  virtual std::shared_ptr<ReactDOMInstance> createHostTextInstance(
      facebook::jsi::Runtime& runtime,
      const std::string& text);

  virtual void appendHostChild(
      std::shared_ptr<ReactDOMInstance> parent,
      std::shared_ptr<ReactDOMInstance> child);

  virtual void removeHostChild(
      std::shared_ptr<ReactDOMInstance> parent,
      std::shared_ptr<ReactDOMInstance> child);

  virtual void insertHostChildBefore(
      std::shared_ptr<ReactDOMInstance> parent,
      std::shared_ptr<ReactDOMInstance> child,
      std::shared_ptr<ReactDOMInstance> beforeChild);

  virtual void commitHostUpdate(
      std::shared_ptr<ReactDOMInstance> instance,
      const facebook::jsi::Object& oldProps,
      const facebook::jsi::Object& newProps,
      const facebook::jsi::Object& payload);

  virtual void commitHostTextUpdate(
      std::shared_ptr<ReactDOMInstance> instance,
      const std::string& oldText,
      const std::string& newText);
};

} // namespace react
