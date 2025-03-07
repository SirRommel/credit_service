#include "rabbitmq_manager.h"
#include <iostream>

#include "rabbitmq_manager.h"
#include <iostream>

RabbitMQManager::RabbitMQManager(const std::map<std::string, std::string>& config)
    : config_(config), running_(true) {
    handler_ = new AMQP::LibBoostAsioHandler(ioc_);
    connect();
    thread_ = std::thread([this]() {
        ioc_.run();
        process_queue();
    });
}

RabbitMQManager::~RabbitMQManager() {
    stop();
    delete channel_;
    delete connection_;
    delete handler_;
}

void RabbitMQManager::connect() {
    std::string host = config_.at("RABBITMQ_HOST");
    int port = std::stoi(config_.at("RABBITMQ_PORT"));
    std::string user = config_.at("RABBITMQ_USER");
    std::string password = config_.at("RABBITMQ_PASSWORD");
    std::string vhost = config_.at("RABBITMQ_VHOST");

    // Исправление: Создаем объект Login
    AMQP::Login login(user, password);
    connection_ = new AMQP::TcpConnection(handler_, AMQP::Address(host, port, login, vhost));
    channel_ = new AMQP::TcpChannel(connection_);

    // Объявляем exchange и очередь, затем запускаем обработку сообщений
    channel_->declareExchange("loan_exchange", AMQP::direct)
        .onSuccess([this]() {
            channel_->declareQueue("loan_requests")
                .onSuccess([this](const std::string& name, uint32_t, uint32_t) {
                    channel_->bindQueue("loan_exchange", name, "loan_routing_key")
                        .onSuccess([this]() {
                            // После успешной привязки запускаем обработку очереди
                            ioc_.post([this] { process_queue(); });
                        });
                });
        });
}

void RabbitMQManager::async_publish(const std::string& message) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    message_queue_.push(message);
    cv_.notify_one();
}

void RabbitMQManager::process_queue() {
    while (running_) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        cv_.wait(lock, [this] { return !message_queue_.empty() || !running_; });

        if (!running_ && message_queue_.empty()) break;

        std::string message = message_queue_.front();
        message_queue_.pop();
        lock.unlock();

        try {
            AMQP::Envelope envelope(message.data(), message.size());
            envelope.setDeliveryMode(2); // persistent

            channel_->publish("loan_exchange", "loan_routing_key", envelope);
        } catch (const std::exception& e) {
            std::cerr << "RabbitMQ publish error: " << e.what() << std::endl;
            connect(); // Переподключение
        }
    }
}

void RabbitMQManager::stop() {
    running_ = false;
    cv_.notify_all();
    if (thread_.joinable()) {
        thread_.join();
    }
    ioc_.stop();
}