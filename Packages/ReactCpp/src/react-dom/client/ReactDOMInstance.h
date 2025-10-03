#pragma once

#include "jsi/jsi.h"
#include <memory>
#include <string>
#include <vector>

namespace react {

class ReactDOMInstance : public facebook::jsi::HostObject {
public:
  virtual ~ReactDOMInstance() = default;

  virtual void appendChild(std::shared_ptr<ReactDOMInstance> child) = 0;
  virtual void removeChild(std::shared_ptr<ReactDOMInstance> child) = 0;
  virtual void insertChildBefore(
    std::shared_ptr<ReactDOMInstance> child,
    std::shared_ptr<ReactDOMInstance> beforeChild) = 0;

  virtual void setAttribute(
    const std::string& key,
    const facebook::jsi::Value& value) = 0;
  virtual void removeAttribute(const std::string& key) = 0;
  virtual facebook::jsi::Value getAttribute(
    facebook::jsi::Runtime& rt,
    const std::string& key) const = 0;

  virtual void setTextContent(const std::string& text) = 0;
  virtual const std::string& getTextContent() const = 0;

  void setKey(std::string key);
  const std::string& getKey() const;

  facebook::jsi::Value get(facebook::jsi::Runtime&, const facebook::jsi::PropNameID& name) override;
  void set(facebook::jsi::Runtime&, const facebook::jsi::PropNameID& name, const facebook::jsi::Value& value) override;
  std::vector<facebook::jsi::PropNameID> getPropertyNames(facebook::jsi::Runtime& rt) override;

  std::string tagName;
  std::string className;
  std::string key;

  std::weak_ptr<ReactDOMInstance> parent;
  std::vector<std::shared_ptr<ReactDOMInstance>> children;

protected:
  void* hostData_{nullptr};
};

} // namespace react
