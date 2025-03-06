#include "rabbit_mq_worker.h"
#include "rabbit_mq_task.h"
#include <amqpcpp.h>
#include <amqpcpp/libboostasio.h>
#include <iostream>

#include "rabbit_mq_worker.h"
#include <iostream>
void RabbitMQWorker::initializeConnection(const std::map<std::string, std::string> &env) {
    try {
        // Настройки подключения к RabbitMQ
        std::string host = env.at("RABBITMQ_HOST");
        int port = std::stoi(env.at("RABBITMQ_PORT"));
        std::string user = env.at("RABBITMQ_USER");
        std::string password = env.at("RABBITMQ_PASSWORD");
        std::string vhost = env.at("RABBITMQ_VHOST");

        boost::asio::ip::tcp::resolver resolver(rabbitmq_io_context_);
        boost::asio::ip::tcp::socket socket(rabbitmq_io_context_);
        boost::asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(host, std::to_string(port));

        boost::asio::connect(socket, endpoints);
        AMQP::LibBoostAsioHandler handler(rabbitmq_io_context_);

        // Создаем уникальные указатели на соединение и канал
        connection_ = std::make_unique<AMQP::TcpConnection>(&handler, AMQP::Address(
            "amqp://" + user + ":" + password + "@" + host + ":" + std::to_string(port) + "/" + vhost));
        channel_ = std::make_unique<AMQP::TcpChannel>(connection_.get());

        // Запускаем io_context в отдельном потоке перед объявлением очереди
        std::thread io_thread([this]() {
            rabbitmq_io_context_.run();
        });
        io_thread.detach();

        // Объявление очереди при инициализации
        (*channel_).declareQueue("my_queue").onSuccess([this]() {
            std::cout << "Queue 'my_queue' declared successfully." << std::endl;
        });
    } catch (const std::exception &e) {
        std::cerr << "RabbitMQWorker initialization error: " << e.what() << std::endl;
    }
}
RabbitMQWorker::RabbitMQWorker(const std::map<std::string, std::string> &env)
    : rabbitmq_io_context_(),
      connection_(nullptr),
      channel_(nullptr) {
    try {
        initializeConnection(env);
    } catch (const std::exception &e) {
        std::cerr << "RabbitMQWorker initialization error: " << e.what() << std::endl;
    }
}



void RabbitMQWorker::start() {
    try {
        boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard(
            rabbitmq_io_context_.get_executor());

        // Запускаем io_context в отдельном потоке
        std::thread io_thread([this]() {
            rabbitmq_io_context_.run();
        });
        io_thread.detach();

        // Запускаем процесс обработки сообщений
        std::thread worker_thread([this]() {
            try {
                processMessages();
            } catch (const std::exception &e) {
                std::cerr << "RabbitMQWorker thread error: " << e.what() << std::endl;
            }
        });
        worker_thread.join();
    } catch (const std::exception &e) {
        std::cerr << "RabbitMQWorker start error: " << e.what() << std::endl;
    }
}

void RabbitMQWorker::enqueueMessage(const std::string &message) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    message_queue_.push(message);
    cv_.notify_one();
}

void RabbitMQWorker::stop() {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        stop_worker_ = true;
    }
    cv_.notify_all();

    rabbitmq_io_context_.stop();
    if (connection_) {
        connection_->close();
    }
}

void RabbitMQWorker::processMessages() {
    try {
        while (!stop_worker_) {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            cv_.wait(lock, [this]() { return !message_queue_.empty() || stop_worker_; });

            if (stop_worker_ && message_queue_.empty()) {
                break;
            }

            std::string message = std::move(message_queue_.front());
            message_queue_.pop();
            lock.unlock();

            if (!message.empty()) {
                (*channel_).declareQueue("my_queue").onSuccess([&]() {
                    (*channel_).publish("", "my_queue", message);
                    std::cout << "Message sent: " << message << std::endl;
                    std::cout << "Queue 'my_queue' declared successfully." << std::endl;
                });

            }
        }
    } catch (const std::exception &e) {
        std::cerr << "RabbitMQWorker processMessages error: " << e.what() << std::endl;
    }
}