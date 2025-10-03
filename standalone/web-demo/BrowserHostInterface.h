#pragma once

#ifdef __EMSCRIPTEN__

#include "runtime/ReactHostInterface.h"
#include "react-dom/client/ReactDOMComponent.h"

#include <emscripten/val.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace facebook {
namespace jsi {
class Runtime;
class Object;
class Value;
} // namespace jsi
} // namespace facebook

namespace react::browser {

class BrowserHostInterface : public HostInterface {
public:
  BrowserHostInterface();
  ~BrowserHostInterface() override;

  void attachRuntime(facebook::jsi::Runtime& runtime);

  std::shared_ptr<ReactDOMInstance> createHostInstance(
      facebook::jsi::Runtime& runtime,
      const std::string& type,
      const facebook::jsi::Object& props) override;

  std::shared_ptr<ReactDOMInstance> createHostTextInstance(
      facebook::jsi::Runtime& runtime,
      const std::string& text) override;

  void appendHostChild(
      std::shared_ptr<ReactDOMInstance> parent,
      std::shared_ptr<ReactDOMInstance> child) override;

  void removeHostChild(
      std::shared_ptr<ReactDOMInstance> parent,
      std::shared_ptr<ReactDOMInstance> child) override;

  void insertHostChildBefore(
      std::shared_ptr<ReactDOMInstance> parent,
      std::shared_ptr<ReactDOMInstance> child,
      std::shared_ptr<ReactDOMInstance> beforeChild) override;

  void commitHostUpdate(
      std::shared_ptr<ReactDOMInstance> instance,
      const facebook::jsi::Object& oldProps,
      const facebook::jsi::Object& newProps,
      const facebook::jsi::Object& payload) override;

  void commitHostTextUpdate(
      std::shared_ptr<ReactDOMInstance> instance,
      const std::string& oldText,
      const std::string& newText) override;

  std::shared_ptr<ReactDOMInstance> createRootContainer(const std::string& elementId);

private:
  void ensureRuntimeAttached();
  emscripten::val document();
  emscripten::val getNode(const std::shared_ptr<ReactDOMInstance>& instance) const;
  emscripten::val getNode(ReactDOMInstance* instance) const;
  void trackInstance(const std::shared_ptr<ReactDOMInstance>& instance, const emscripten::val& node);
  void untrackInstance(ReactDOMInstance* instance);

  void applyInitialProps(
      const facebook::jsi::Object& props,
      const emscripten::val& element);
  void applyAttribute(
      const std::string& key,
      const facebook::jsi::Value& value,
      const emscripten::val& element);
  void removeAttribute(
      const std::string& key,
      const emscripten::val& element);
    void applyPayload(
            const facebook::jsi::Object& payload,
            const emscripten::val& element);

  facebook::jsi::Runtime* runtime_{nullptr};
  std::unordered_map<ReactDOMInstance*, emscripten::val> nodes_;
};

} // namespace react::browser

#endif // __EMSCRIPTEN__
