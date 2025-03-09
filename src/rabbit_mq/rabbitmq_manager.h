#ifndef RABBITMQ_MANAGER_H
#define RABBITMQ_MANAGER_H

#include <amqpcpp.h>
#include <amqpcpp/libboostasio.h>
#include <boost/asio.hpp>
#include <queue>
#include <mutex>
#include <thread>
#include <map>
#include <boost/property_tree/ptree_fwd.hpp>

#include "db/database_manager.h"

class RabbitMQManager {
public:
    RabbitMQManager(const std::map<std::string, std::string>& config, db::DatabaseManager& db);
    ~RabbitMQManager();

    void async_publish(const std::string& message);
    void start_consuming();
    void stop();
    std::optional<boost::property_tree::ptree> wait_for_response(
        const std::string& type,
        std::chrono::seconds timeout,
        const std::map<std::string, std::string>& filters);


private:
    void connect();
    void setup_consumer();
    void process_queue();
    void add_credit_to_db(const auto& json);
    void add_pay(const auto& json);
    void register_handler(const std::string& type,
                                  std::function<void(const boost::property_tree::ptree&)> handler);
    void send_periodic_message();
    void start_periodic_timer();


    boost::asio::io_context ioc_;
    boost::asio::steady_timer periodic_timer_;
    std::map<std::string, std::function<void(const boost::property_tree::ptree&)>> handlers_;
    std::mutex handlers_mutex_;
    db::DatabaseManager& db_;
    std::map<std::string, std::deque<boost::property_tree::ptree>> processed_message_queues_;
    std::condition_variable queue_cv_;
    std::map<std::string, std::string> config_;
    std::thread thread_;
    std::thread publish_thread_;
    AMQP::LibBoostAsioHandler* handler_;
    AMQP::TcpConnection* connection_;
    AMQP::TcpChannel* channel_;

    std::queue<std::string> message_queue_;
    std::mutex queue_mutex_;
    std::condition_variable cv_;
    bool running_;
};

#endif // RABBITMQ_MANAGER_H