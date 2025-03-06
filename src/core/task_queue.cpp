#include "task_queue.h"
#include <iostream>
#include <functional>
void TaskQueue::enqueueTask(TaskPtr task) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    tasks_.push(task);
    cv_.notify_one();
}

TaskQueue::TaskPtr TaskQueue::dequeueTask() {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    cv_.wait(lock, [this]() { return !tasks_.empty() || stopped_; });

    if (stopped_ && tasks_.empty()) {
        return nullptr;
    }

    TaskPtr task = std::move(tasks_.front());
    tasks_.pop();
    return task;
}

void TaskQueue::stop() {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        stopped_ = true;
    }
    cv_.notify_all();
}

bool TaskQueue::isStopped() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return stopped_;
}