#ifndef RABBITMQ_MANAGER_H
#define RABBITMQ_MANAGER_H

#include <amqpcpp.h>
#include <amqpcpp/libboostasio.h>
#include <boost/asio.hpp>
#include <queue>
#include <mutex>
#include <thread>
#include <map>

class RabbitMQManager {
public:
    RabbitMQManager(const std::map<std::string, std::string>& config);
    ~RabbitMQManager();

    void async_publish(const std::string& message);
    void start_consuming();
    void stop();

private:
    void connect();
    void setup_consumer();
    void process_queue();

    std::map<std::string, std::string> config_;
    boost::asio::io_context ioc_;
    std::thread thread_;
    std::thread publish_thread_; // Отдельный поток для публикаций
    AMQP::LibBoostAsioHandler* handler_;
    AMQP::TcpConnection* connection_;
    AMQP::TcpChannel* channel_;

    std::queue<std::string> message_queue_;
    std::mutex queue_mutex_;
    std::condition_variable cv_;
    bool running_;
};

#endif // RABBITMQ_MANAGER_H