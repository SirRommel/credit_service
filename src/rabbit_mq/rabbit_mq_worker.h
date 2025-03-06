#ifndef CREDIT_SERVICE_RABBITMQWORKER_H
#define CREDIT_SERVICE_RABBITMQWORKER_H

#include "../core/task_queue.h"
#include <boost/asio.hpp>
#include <amqpcpp.h>
#include <amqpcpp/libboostasio.h>
#include <map>
#include <string>
#include <thread>
#include <condition_variable>
#include <queue>

class RabbitMQWorker {
public:
    explicit RabbitMQWorker(const std::map<std::string, std::string> &env);

    void start(); // Запускает рабочий поток
    void enqueueMessage(const std::string &message); // Добавляет сообщение в очередь
    void stop(); // Останавливает рабочий поток

private:
    boost::asio::io_context rabbitmq_io_context_;
    std::unique_ptr<AMQP::TcpConnection> connection_; // Указатель на соединение
    std::unique_ptr<AMQP::TcpChannel> channel_; // Указатель на канал

    std::queue<std::string> message_queue_; // Очередь сообщений
    std::mutex queue_mutex_; // Мьютекс для очереди
    std::condition_variable cv_; // Условная переменная для очереди
    bool stop_worker_ = false; // Флаг завершения работы

    void initializeConnection(const std::map<std::string, std::string> &env); // Инициализация соединения и очереди
    void processMessages(); // Обработка сообщений из очереди
};
#endif //CREDIT_SERVICE_RABBITMQWORKER_H