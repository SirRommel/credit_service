#include "rabbit_mq_task.h"
#include <amqpcpp.h>
#include <amqpcpp/libboostasio.h>
#include <iostream>

RabbitMQTask::RabbitMQTask(const std::string &message, const std::map<std::string, std::string> &env)
    : message_(message), env_(env) {}

void RabbitMQTask::execute(boost::asio::io_context &io_context) {
    try {
        boost::asio::ip::tcp::resolver resolver(io_context);
        boost::asio::ip::tcp::socket socket(io_context);

        std::string host = env_["RABBITMQ_HOST"];
        int port = std::stoi(env_["RABBITMQ_PORT"]);
        std::string user = env_["RABBITMQ_USER"];
        std::string password = env_["RABBITMQ_PASSWORD"];
        std::string vhost = env_["RABBITMQ_VHOST"];

        boost::asio::connect(socket, resolver.resolve(host, std::to_string(port)));
        AMQP::LibBoostAsioHandler handler(io_context);
        AMQP::TcpConnection connection(&handler, AMQP::Address("amqp://" + user + ":" + password + "@" + host + ":" + std::to_string(port) + "/" + vhost));
        AMQP::TcpChannel channel(&connection);

        channel.declareQueue("my_queue").onSuccess([&]() {
            channel.publish("", "my_queue", message_);
            std::cout << "Message sent: " << message_ << std::endl;
        });

        io_context.run();
    } catch (const std::exception &e) {
        std::cerr << "RabbitMQTask error: " << e.what() << std::endl;
    }
}