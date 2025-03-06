#ifndef CREDIT_SERVICE_RABBITMQHANDLER_H
#define CREDIT_SERVICE_RABBITMQHANDLER_H

#include <iostream>
#include <boost/asio.hpp>
#include <memory>
#include <string>

class RabbitMQWorker;

class RabbitMQHandler {
public:
    explicit RabbitMQHandler(std::shared_ptr<RabbitMQWorker> worker);

    void handleSendToRabbit(boost::asio::ip::tcp::socket &socket, std::istream &request_stream);

private:
    std::shared_ptr<RabbitMQWorker> worker_;
};

#endif //CREDIT_SERVICE_RABBITMQHANDLER_H