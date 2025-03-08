#include "rabbitmq_manager.h"
#include <iostream>

#include "tools/json_tools.h"

RabbitMQManager::RabbitMQManager(const std::map<std::string, std::string>& config)
    : config_(config), running_(true) {
    handler_ = new AMQP::LibBoostAsioHandler(ioc_);
    connect();
    // Запускаем io_context в отдельном потоке
    thread_ = std::thread([this]() { ioc_.run(); });
    // Запускаем обработку очереди публикаций в отдельном потоке
    publish_thread_ = std::thread([this]() { process_queue(); });

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
            channel_->declareQueue("swag", AMQP::durable)
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
        std::string body(message.body(), message.bodySize());
        try {
            liid(body);
            auto json = parse_rabbitmq_message(body);
            std::string type = json.get<std::string>("type_message", "other");

            std::lock_guard<std::mutex> lock(queue_mutex_);
            message_queues_[type].push_back(std::move(json));
            queue_cv_.notify_all();
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

std::optional<boost::property_tree::ptree> RabbitMQManager::wait_for_response(
    const std::string& type,
    std::chrono::seconds timeout,
    const std::map<std::string, std::string>& filters)
{
    const auto start_time = std::chrono::steady_clock::now();
    std::unique_lock<std::mutex> lock(queue_mutex_);

    const std::string queue_key = type.empty() ? "other" : type;


    while (std::chrono::steady_clock::now() - start_time < timeout) {
        auto& queue = message_queues_[queue_key];
        // Используем индекс для быстрого доступа
        for (auto it = queue.begin(); it != queue.end();) {
            try {
                // Проверка фильтров
                bool match = true;
                for (const auto& [key, value] : filters) {
                    if (it->get<std::string>(key) != value) {
                        match = false;
                        break;
                    }
                }

                if (match) {
                    auto message = *it;
                    it = queue.erase(it); // Безопасное удаление
                    return message;
                }
                ++it;

            } catch (const std::exception& e) {
                // Удаляем поврежденные сообщения
                it = queue.erase(it);
                std::cerr << "Malformed message removed: " << e.what() << std::endl;
            }
        }

        // Ожидание с минимальной блокировкой
        if (queue_cv_.wait_for(lock, timeout) == std::cv_status::timeout) {
            break;
        }
    }

    return std::nullopt;
}