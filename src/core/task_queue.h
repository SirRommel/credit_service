#ifndef CREDIT_SERVICE_TASKQUEUE_H
#define CREDIT_SERVICE_TASKQUEUE_H

#include "../models/task.h"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>

class TaskQueue {
public:
    using TaskPtr = std::shared_ptr<Task>;

    void enqueueTask(TaskPtr task);
    TaskPtr dequeueTask();

    void stop();
    bool isStopped() const;

private:
    std::queue<TaskPtr> tasks_;
    mutable std::mutex queue_mutex_;
    std::condition_variable cv_;
    bool stopped_ = false;
};

#endif //CREDIT_SERVICE_TASKQUEUE_H