#include "runtime/ReactHostInterface.h"

#include "react-dom/client/ReactDOMComponent.h"

namespace react {

std::shared_ptr<ReactDOMInstance> HostInterface::createHostInstance(
    facebook::jsi::Runtime& runtime,
    const std::string& type,
    const facebook::jsi::Object& props) {
  auto instance = std::make_shared<ReactDOMComponent>(runtime, type, props);
  instance->tagName = type;
  return instance;
}

std::shared_ptr<ReactDOMInstance> HostInterface::createHostTextInstance(
    facebook::jsi::Runtime& runtime,
    const std::string& text) {
  auto instance = std::make_shared<ReactDOMComponent>(runtime, "#text", text);
  instance->tagName = "#text";
  return instance;
}

void HostInterface::appendHostChild(
    std::shared_ptr<ReactDOMInstance> parent,
    std::shared_ptr<ReactDOMInstance> child) {
  if (parent) {
    parent->appendChild(std::move(child));
  }
}

void HostInterface::removeHostChild(
    std::shared_ptr<ReactDOMInstance> parent,
    std::shared_ptr<ReactDOMInstance> child) {
  if (parent) {
    parent->removeChild(std::move(child));
  }
}

void HostInterface::insertHostChildBefore(
    std::shared_ptr<ReactDOMInstance> parent,
    std::shared_ptr<ReactDOMInstance> child,
    std::shared_ptr<ReactDOMInstance> beforeChild) {
  if (parent) {
    parent->insertChildBefore(std::move(child), std::move(beforeChild));
  }
}

void HostInterface::commitHostUpdate(
    std::shared_ptr<ReactDOMInstance> instance,
    const facebook::jsi::Object& oldProps,
    const facebook::jsi::Object& newProps,
    const facebook::jsi::Object& payload) {
  (void)oldProps;
  auto component = std::dynamic_pointer_cast<ReactDOMComponent>(instance);
  if (!component) {
    return;
  }
  component->applyUpdate(newProps, payload);
}

void HostInterface::commitHostTextUpdate(
    std::shared_ptr<ReactDOMInstance> instance,
    const std::string& oldText,
    const std::string& newText) {
  (void)oldText;
  if (!instance) {
    return;
  }
  instance->setTextContent(newText);
}

} // namespace react
