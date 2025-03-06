#ifndef CREDIT_SERVICE_HTTPROUTER_H
#define CREDIT_SERVICE_HTTPROUTER_H

#include <boost/asio.hpp>
#include <memory>
#include <string>
#include <.//rabbit_mq/rabbit_mq_worker.h>

class HttpRouter {
public:
    HttpRouter(boost::asio::io_context &io_context, const std::string &host, uint16_t port,
               std::shared_ptr<RabbitMQWorker> rabbitmq_worker);

    void start(); // Запускает сервер

private:
    boost::asio::ip::tcp::acceptor acceptor_;
    std::shared_ptr<RabbitMQWorker> rabbitmq_worker_;

    void doAccept();
    void handleRequest(boost::asio::ip::tcp::socket socket);
    void handleSendToRabbit(boost::asio::ip::tcp::socket &socket, std::istream &request_stream);
};

#endif //CREDIT_SERVICE_HTTPROUTER_H