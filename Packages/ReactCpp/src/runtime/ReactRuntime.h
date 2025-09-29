#pragma once

#include "ReactFiberWorkLoop.h"
#include <chrono>
#include <functional>
#include <unordered_map>

namespace react {

class ReactRuntime final : public ReactFiberWorkLoop, public ReactDOMHostConfig, public Scheduler {
public:
	ReactRuntime();
	~ReactRuntime() override = default;

	// --- ReactDOMHostConfig Implementation ---
	std::shared_ptr<ReactDOMInstance> createInstance(
		facebook::jsi::Runtime& rt,
		const std::string& type,
		const facebook::jsi::Object& props) override;

	std::shared_ptr<ReactDOMInstance> createTextInstance(
		facebook::jsi::Runtime& rt,
		const std::string& text) override;

	void appendChild(
			std::shared_ptr<ReactDOMInstance> parent,
			std::shared_ptr<ReactDOMInstance> child) override;

	void removeChild(
			std::shared_ptr<ReactDOMInstance> parent,
			std::shared_ptr<ReactDOMInstance> child) override;

	void insertBefore(
			std::shared_ptr<ReactDOMInstance> parent,
			std::shared_ptr<ReactDOMInstance> child,
			std::shared_ptr<ReactDOMInstance> beforeChild) override;

	void commitUpdate(
		std::shared_ptr<ReactDOMInstance> instance,
		const facebook::jsi::Object& oldProps,
		const facebook::jsi::Object& newProps,
		const facebook::jsi::Object& updatePayload) override;

	// --- Scheduler Implementation ---
	TaskHandle scheduleTask(
		SchedulerPriority priority,
		Task task,
		const TaskOptions& options = {}) override;
	void cancelTask(TaskHandle handle) override;
	SchedulerPriority getCurrentPriorityLevel() const override;
	SchedulerPriority runWithPriority(
		SchedulerPriority priority,
		const std::function<void()>& fn) override;
	bool shouldYield() const override;
	double now() const override;

private:
	struct ScheduledTaskRecord {
		Task task;
		SchedulerPriority priority{SchedulerPriority::NormalPriority};
		TaskOptions options;
		bool cancelled{false};
	};

	void runPendingTask(TaskHandle handle);

	uint64_t nextTaskId_{1};
	SchedulerPriority currentPriorityLevel_{SchedulerPriority::NormalPriority};
	std::unordered_map<uint64_t, ScheduledTaskRecord> pendingTasks_;
	std::chrono::steady_clock::time_point schedulerEpoch_;
};

using ReactRuntimeTestHelper = ReactFiberWorkLoopTestHelper;

} // namespace react
