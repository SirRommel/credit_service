#include "rabbitmq_manager.h"
#include <iostream>

RabbitMQManager::RabbitMQManager(const std::map<std::string, std::string>& config)
    : config_(config), running_(true) {
    handler_ = new AMQP::LibBoostAsioHandler(ioc_);
    connect();
    // Запускаем io_context в отдельном потоке
    thread_ = std::thread([this]() { ioc_.run(); });
    // Запускаем обработку очереди публикаций в отдельном потоке
    publish_thread_ = std::thread([this]() { process_queue(); });
    start_consuming();
}

RabbitMQManager::~RabbitMQManager() {
    stop();
    delete channel_;
    delete connection_;
    delete handler_;
    if (thread_.joinable()) {
        thread_.join();
    }
    if (publish_thread_.joinable()) {
        publish_thread_.join();
    }
}

void RabbitMQManager::connect() {
    std::string host = config_.at("RABBITMQ_HOST");
    int port = std::stoi(config_.at("RABBITMQ_PORT"));
    std::string user = config_.at("RABBITMQ_USER");
    std::string password = config_.at("RABBITMQ_PASSWORD");
    std::string vhost = config_.at("RABBITMQ_VHOST");

    AMQP::Login login(user, password);
    connection_ = new AMQP::TcpConnection(handler_, AMQP::Address(host, port, login, vhost));
    channel_ = new AMQP::TcpChannel(connection_);

    // Объявляем exchange и очередь для loan_requests
    channel_->declareExchange("loan_exchange", AMQP::direct)
        .onSuccess([this]() {
            channel_->declareQueue("loan_requests")
                .onSuccess([this](const std::string& name, uint32_t, uint32_t) {
                    channel_->bindQueue("loan_exchange", name, "loan_routing_key");
                });
        });

    // Объявляем exchange и очередь для swag с перезапуском потребителя
    channel_->declareExchange("swag_exchange", AMQP::direct)
        .onSuccess([this]() {
            channel_->declareQueue("swag")
                .onSuccess([this](const std::string& name, uint32_t, uint32_t) {
                    channel_->bindQueue("swag_exchange", name, "swag_routing_key")
                        .onSuccess([this]() {
                            ioc_.post([this] { setup_consumer(); });
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
            std::cerr << "Publish error: " << e.what() << std::endl;
            // Переподключаемся и повторяем попытку
            connect();
            std::this_thread::sleep_for(std::chrono::seconds(1));
            async_publish(message); // Повторная отправка
        }
    }
}

void RabbitMQManager::stop() {
    running_ = false;
    cv_.notify_all();
    ioc_.stop();
    if (thread_.joinable()) thread_.join();
    if (publish_thread_.joinable()) publish_thread_.join();
}
void liid(const std::string& message) {
    std::cout << "Custom handler: " << message << std::endl;
    // Ваша логика обработки сообщений
}
void RabbitMQManager::setup_consumer() {
    if (!channel_) return;

    // Используем ссылку, чтобы избежать копирования
    auto& consumer = channel_->consume("swag");
    consumer.onReceived([this](const AMQP::Message &message, uint64_t deliveryTag, bool) {
        try {
            std::string body(message.body(), message.bodySize());
            liid(body); // Вызов пользовательской функции
            channel_->ack(deliveryTag);
        } catch (const std::exception& e) {
            std::cerr << "Message handling error: " << e.what() << std::endl;
            // Используем reject с requeue = true
            channel_->reject(deliveryTag, true); // Перемещаем сообщение в очередь повторной обработки
        }
    });
    consumer.onError([this](const char *msg) {
        std::cerr << "Consumer error: " << msg << std::endl;
        connect(); // Переподключаемся и перезапускаем потребителя
        ioc_.post([this] { setup_consumer(); });
    });
}

void RabbitMQManager::start_consuming() {
    ioc_.post([this] { setup_consumer(); });
}