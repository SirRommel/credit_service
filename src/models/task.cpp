#ifndef CREDIT_SERVICE_RABBITMQTASK_H
#define CREDIT_SERVICE_RABBITMQTASK_H

#include "task.h"
#include <string>
#include <map>

class RabbitMQTask : public Task {
public:
    explicit RabbitMQTask(const std::string &message, const std::map<std::string, std::string> &env);

    void execute(boost::asio::io_context &io_context) override;

private:
    std::string message_;
    std::map<std::string, std::string> env_;
};

#endif //CREDIT_SERVICE_RABBITMQTASK_H