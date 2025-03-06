#ifndef CREDIT_SERVICE_TASK_H
#define CREDIT_SERVICE_TASK_H

#include <boost/asio.hpp>
#include <memory>

class Task {
public:
    virtual ~Task() = default;

    // Выполнение задачи
    virtual void execute(boost::asio::io_context &io_context) = 0;
};

#endif //CREDIT_SERVICE_TASK_H