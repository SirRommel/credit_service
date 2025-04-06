#include "rabbitmq_manager.h"
#include <iostream>

#include "tools/json_tools.h"

RabbitMQManager::RabbitMQManager(const std::map<std::string, std::string>& config, db::DatabaseManager& db)
    : config_(config), db_(db), running_(true), periodic_timer_(ioc_, boost::asio::chrono::seconds(30)) {
    handler_ = new AMQP::LibBoostAsioHandler(ioc_);
    connect();
    // Запускаем io_context в отдельном потоке
    thread_ = std::thread([this]() { ioc_.run(); });
    // Запускаем обработку очереди публикаций в отдельном потоке
    publish_thread_ = std::thread([this]() { process_queue(); });
    register_handler("write_off_money", [&](const auto& json) {
            this->add_pay(json);
        });
    register_handler("creation_credit", [&](const auto& json) {
        this->add_credit_to_db(json);
    });

    start_periodic_timer();
}

RabbitMQManager::~RabbitMQManager() {
    try {
        running_ = false;
        periodic_timer_.cancel();
        cv_.notify_all();
        ioc_.stop();
        if (thread_.joinable()) thread_.join();
        if (publish_thread_.joinable()) publish_thread_.join();
        delete channel_;
        delete connection_;
        delete handler_;
        std::cout << std::endl << "RabbitMQ manager stoped." << std::endl;
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

void RabbitMQManager::reconnect() {

    if (reconnecting_) return;
    reconnecting_ = true;
    std::cout << "Reconnecting in 5 seconds..." << std::endl;
    ioc_.post([this] {
        delete channel_;
        delete connection_;
        connect();
        reconnecting_ = false;
    });
}

void RabbitMQManager::stop() {
    running_ = false;
    periodic_timer_.cancel();
    cv_.notify_all();
    ioc_.stop();
    if (thread_.joinable()) thread_.join();
    if (publish_thread_.joinable()) publish_thread_.join();
}


void RabbitMQManager::connect() {
    std::string host = config_.at("RABBITMQ_HOST");
    int port = std::stoi(config_.at("RABBITMQ_PORT"));
    std::string user = config_.at("RABBITMQ_USER");
    std::string password = config_.at("RABBITMQ_PASSWORD");
    std::string vhost = config_.at("RABBITMQ_VHOST");

    std::string sending_exchange = config_.at("RABBITMQ_SENDING_EXCHANGE");
    std::string sending_key = config_.at("RABBITMQ_SENDING_ROUTING_KEY");
    std::string sending_queue = config_.at("RABBITMQ_SENDING_QUEUE");

    std::string listening_exchange = config_.at("RABBITMQ_LISTENING_EXCHANGE");
    std::string listening_key = config_.at("RABBITMQ_LISTENING_ROUTING_KEY");
    std::string listening_queue = config_.at("RABBITMQ_LISTENING_QUEUE");

    AMQP::Login login(user, password);
    connection_ = new AMQP::TcpConnection(handler_, AMQP::Address(host, port, login, vhost));
    channel_ = new AMQP::TcpChannel(connection_);


    // Обработчик ошибок канала
    channel_->onError([this](const char* msg) {
        std::cerr << "Channel error: " << msg << std::endl;
        reconnect();
    });

    // Для отправки
    channel_->declareExchange(sending_exchange, AMQP::direct, AMQP::durable)
            .onSuccess([this, sending_queue, sending_exchange, sending_key]() {
                channel_->declareQueue(sending_queue, AMQP::durable)
                        .onSuccess([this, sending_exchange, sending_key](const std::string& name, uint32_t, uint32_t) {
                            channel_->bindQueue(sending_exchange, name, sending_key)
                                    .onError([](const char* msg) {
                                        std::cerr << "Bind error (send): " << msg << std::endl;
                                    });
                        })
                        .onError([](const char* msg) {
                            std::cerr << "Queue declare error (send): " << msg << std::endl;
                        });
            })
            .onError([](const char* msg) {
                std::cerr << "Exchange declare error (send): " << msg << std::endl;
            });

    // Для приема
    channel_->declareExchange(listening_exchange, AMQP::direct, AMQP::durable)
            .onSuccess([this, listening_queue, listening_exchange, listening_key]() {
                channel_->declareQueue(listening_queue, AMQP::durable)
                        .onSuccess([this, listening_exchange, listening_key](const std::string& name, uint32_t, uint32_t) {
                            channel_->bindQueue(listening_exchange, name, listening_key)
                                    .onSuccess([this]() {
                                        ioc_.post([this] { setup_consumer(); });
                                    })
                                    .onError([](const char* msg) {
                                        std::cerr << "Bind error (listen): " << msg << std::endl;
                                    });
                        })
                        .onError([](const char* msg) {
                            std::cerr << "Queue declare error (listen): " << msg << std::endl;
                        });
            })
            .onError([](const char* msg) {
                std::cerr << "Exchange declare error (listen): " << msg << std::endl;
            });
}

void RabbitMQManager::start_periodic_timer() {
    periodic_timer_.async_wait([this](const boost::system::error_code&) {
        send_periodic_message();
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
        std::string sending_exchange = config_.at("RABBITMQ_SENDING_EXCHANGE");
        std::string sending_key = config_.at("RABBITMQ_SENDING_ROUTING_KEY");

        try {
            AMQP::Envelope envelope(message.data(), message.size());
            envelope.setDeliveryMode(2); // persistent
            channel_->publish(sending_exchange, sending_key, envelope);
        } catch (const std::exception& e) {
            std::cerr << "Publish error: " << e.what() << std::endl;
            connect();
            std::this_thread::sleep_for(std::chrono::seconds(1));
            async_publish(message); // Повторная отправка
        }
    }
}


void liid(const std::string& message) {
    std::cout << "Custom handler: " << message << std::endl;
}


void RabbitMQManager::setup_consumer() {
    if (!channel_) return;
    std::string listening_queue = config_.at("RABBITMQ_LISTENING_QUEUE");

    auto& consumer = channel_->consume(listening_queue);
    consumer.onReceived([this](const AMQP::Message &message, uint64_t deliveryTag, bool) {
        std::string body(message.body(), message.bodySize());
        try {
            liid(body);
            auto json = parse_rabbitmq_message(body);
            std::string type = json.get<std::string>("type_message", "other");

            bool handled = false;

            int retry_count = json.get("retry_count", 0);
            if (retry_count > 3) {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                processed_message_queues_["dead_letters"].push_back(json);
                channel_->ack(deliveryTag);
                return;
            }
            std::cout << type << " KUNI" << std::endl;
            {
                std::lock_guard<std::mutex> lock(handlers_mutex_);
                auto it = handlers_.find(type);
                if (it != handlers_.end()) {
                    try {
                        std::cout << type << " handler already exists" << std::endl;
                        it->second(json);
                        handled = true;
                        std::lock_guard<std::mutex> queue_lock(queue_mutex_);
                        processed_message_queues_[type].push_back(json);
                        queue_cv_.notify_all();
                    } catch (const std::exception& e) {
                        json.put("retry_count", retry_count + 1);
                        processed_message_queues_["handler_errors"].push_back(json);
                        throw;
                    }
                }
            }

            if (!handled) {
                json.put("retry_count", retry_count + 1);
                std::lock_guard<std::mutex> queue_lock(queue_mutex_);
                processed_message_queues_["unhandled"].push_back(json);
                queue_cv_.notify_all();
            }

            channel_->ack(deliveryTag);
        } catch (const std::exception& e) {
            std::cerr << "Message handling error: " << e.what() << std::endl;
            channel_->reject(deliveryTag, true);
        }
    }
    );
    consumer.onError([this](const char *msg) {
        std::cerr << "Consumer error: " << msg << std::endl;
        connect();
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
        auto& main_queue = processed_message_queues_[queue_key];
        for (auto it = main_queue.begin(); it != main_queue.end();) {
            try {
                bool match = true;
                for (const auto& [key, value] : filters) {
                    if (it->get<std::string>(key) != value) {
                        match = false;
                        break;
                    }
                }
                if (match) {
                    auto message = *it;
                    it = main_queue.erase(it);
                    return message;
                }
                ++it;
            } catch (const std::exception& e) {
                it = main_queue.erase(it);
                std::cerr << "Malformed message removed: " << e.what() << std::endl;
            }
        }

        // Проверяем очередь ошибок
        auto& error_queue = processed_message_queues_["handler_errors"];
        for (auto it = error_queue.begin(); it != error_queue.end();) {
            try {
                bool match = true;
                for (const auto& [key, value] : filters) {
                    if (it->get<std::string>(key) != value) {
                        match = false;
                        break;
                    }
                }
                if (match) {
                    auto message = *it;
                    it = error_queue.erase(it);
                    return message; // Возвращаем ошибочное сообщение
                }
                ++it;
            } catch (const std::exception& e) {
                it = error_queue.erase(it);
                std::cerr << "Malformed error message removed: " << e.what() << std::endl;
            }
        }

        if (queue_cv_.wait_for(lock, timeout) == std::cv_status::timeout) {
            break;
        }
    }

    return std::nullopt;
}

void RabbitMQManager::register_handler(const std::string& type,
                                      std::function<void(const boost::property_tree::ptree&)> handler) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    handlers_[type] = handler;
}

void RabbitMQManager::add_credit_to_db(const auto& json) {
    std::string user_id_str = json.template get<std::string>("user_id");
    std::string tariff_id_str = json.template get<std::string>("tariff_id");
    boost::optional<std::string> write_off_account_id_str =
        json.template get_optional<std::string>("write_off_account_id");
    double amount = json.template get<double>("amount");
    std::string amount_str = std::to_string(amount);
    bool success = json.template get<bool>("success", false);
    if (!success) {
        boost::property_tree::ptree error_json = json;
        error_json.put("error", json.template get<std::string>("error", "Credit creation failed"));
        error_json.put("status", "failed");
        error_json.put("user_id", user_id_str);
        error_json.put("tariff_id", tariff_id_str);
        error_json.put("write_off_account_id", write_off_account_id_str);
        error_json.put("amount", amount_str);
        error_json.put("success", success);

        // Помещаем в очередь ошибок
        std::lock_guard<std::mutex> lock(queue_mutex_);
        processed_message_queues_["handler_errors"].push_back(error_json);
        queue_cv_.notify_all();
        return;
        std::string error_msg = json.template get<std::string>("error", "Credit creation failed");
        throw std::runtime_error(error_msg);
    }


    const char* paramValues[5];
    paramValues[0] = user_id_str.c_str();
    paramValues[1] = tariff_id_str.c_str();
    paramValues[2] = amount_str.c_str();
    paramValues[3] = write_off_account_id_str ?
            write_off_account_id_str->c_str() : nullptr;
    paramValues[4] = amount_str.c_str();
    for (int i = 0; i < 4; i++) {
        std::cout << paramValues[i] << std::endl;
    }

    std::promise<PGresult*> db_promise;
    db_.async_query_params(
        "INSERT INTO credits (user_id, tariff_id, amount, write_off_account_id, remaining_debt) "
        "VALUES ($1, $2, $3, $4, $5)",
        paramValues,
        5,
        [&](PGresult* res) { db_promise.set_value(res); }
    );

    PGresult* db_result = db_promise.get_future().get();
    if (PQresultStatus(db_result) != PGRES_COMMAND_OK) {
        std::string error = PQerrorMessage(db_.get_connection());
        PQclear(db_result);
        throw std::runtime_error("Database error: " + error);
    }
    PQclear(db_result);
}
std::string get_current_date() {
    // Получаем текущее время
    auto now = std::chrono::system_clock::now();

    // Конвертируем в time_t для работы с датой
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);

    // Преобразуем time_t в строку
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_time), "%Y-%m-%d %H:%M:%S"); // Формат "YYYY-MM-DD HH:MM:SS"

    return ss.str();
}

void RabbitMQManager::add_pay(const auto& json) {
    try {
        std::cout << "add_pay fdfwfewwggggwegwgweFFFFFFFFFFFFFFFFFF" << std::endl;
        std::string user_id = json.template get<std::string>("user_id");
        double amount = json.template get<double>("amount");
        std::string summ_str = std::to_string(amount);
        bool success = json.template get<bool>("success", false);
        std::string success_str = std::to_string(success);
        // обновляем рейтинг в credit_history
        std::string update_rating_query =
            "UPDATE credit_history "
            "SET rating = CASE "
                "WHEN $1 THEN LEAST(rating + 1, 100) "
                "ELSE GREATEST(rating - 1, 0) "
            "END "
            "WHERE user_id = $2";

        const char* paramValues[4];
        paramValues[0] = user_id.c_str();
        paramValues[1] = summ_str.c_str();
        paramValues[2] = success_str.c_str();
        std::string current_date = get_current_date();
        paramValues[3] = current_date.c_str();
        std::promise<PGresult*> db_promise;
        db_.async_query_params(
            "INSERT INTO credit_payments (user_id, summ, status, payment_date) "
            "VALUES ($1, $2, $3, $4)",
            paramValues,
            4,
            [&](PGresult* res) { db_promise.set_value(res); }
        );
        PGresult* db_result = db_promise.get_future().get();
        if (PQresultStatus(db_result) != PGRES_COMMAND_OK) {
            std::string error = PQerrorMessage(db_.get_connection());
            PQclear(db_result);
            throw std::runtime_error("Database error: " + error);
        }
        PQclear(db_result);

        std::promise<PGresult*> rating_promise;
        const char* rating_params[] = {
            success ? "true" : "false",
            user_id.c_str()
        };
        db_.async_query_params(
            update_rating_query,
            rating_params,
            2,
            [&](PGresult* res) { rating_promise.set_value(res); }
        );

        PGresult* rating_result = rating_promise.get_future().get();
        if (PQresultStatus(rating_result) != PGRES_COMMAND_OK) {
            throw std::runtime_error("Rating update failed: " +
                std::string(PQerrorMessage(db_.get_connection())));
        }
        PQclear(rating_result);

        // если успешный платеж - обновляем остаток долга
        if (success) {
            std::string update_debt_query =
                "UPDATE credits "
                "SET remaining_debt = GREATEST(remaining_debt - $1, 0) "
                "WHERE user_id = $2 AND remaining_debt >= $1";

            std::string amount_str = std::to_string(amount);

            const char* debt_params[] = {
                amount_str.c_str(),
                user_id.c_str()
            };

            std::promise<PGresult*> debt_promise;
            db_.async_query_params(
                update_debt_query,
                debt_params,
                2,
                [&](PGresult* res) { debt_promise.set_value(res); }
            );

            PGresult* debt_result = debt_promise.get_future().get();
            if (PQresultStatus(debt_result) != PGRES_COMMAND_OK) {
                throw std::runtime_error("Debt update failed: " +
                    std::string(PQerrorMessage(db_.get_connection())));
            }
            PQclear(debt_result);
        }

        std::cout << "Processed payment for user " << user_id
                  << " (success: " << std::boolalpha << success << ")"
                  << std::endl;

    } catch (const std::exception& e) {

        boost::property_tree::ptree error_json = json;
        error_json.put("error", e.what());
        error_json.put("status", "failed");

        std::lock_guard<std::mutex> lock(queue_mutex_);
        processed_message_queues_["handler_errors"].push_back(error_json);
        queue_cv_.notify_all();
    }
}

void RabbitMQManager::send_periodic_message() {
    try {
        std::promise<PGresult*> db_promise;
        db_.async_query_params(
            "SELECT c.user_id, c.write_off_account_id, t.interest_rate, "
                   "c.amount AS total_amount, c.remaining_debt, t.months_count "
            "FROM credits c "
            "JOIN tariffs t ON c.tariff_id = t.id "
            "WHERE c.remaining_debt > 0",
            nullptr, 0,
            [&](PGresult* res) { db_promise.set_value(res); }
        );

        PGresult* db_result = db_promise.get_future().get();
        if (PQresultStatus(db_result) != PGRES_TUPLES_OK) {
            throw std::runtime_error("DB error: " + std::string(PQerrorMessage(db_.get_connection())));
        }

        int rows = PQntuples(db_result);
        for (int i = 0; i < rows; ++i) {
            // парс данных из БД
            std::string user_id = PQgetvalue(db_result, i, 0);
            std::string account_id = PQgetvalue(db_result, i, 1);
            double interest_rate = std::stod(PQgetvalue(db_result, i, 2));
            double total_amount = std::stod(PQgetvalue(db_result, i, 3));
            double remaining_debt = std::stod(PQgetvalue(db_result, i, 4));
            int months = std::stoi(PQgetvalue(db_result, i, 5));

            // рассчитать сумму списания
            double monthly_rate = interest_rate / 12 / 100;
            double payment = (total_amount * monthly_rate) /
                            (1 - std::pow(1 + monthly_rate, -months));

            double amount = std::min(payment, remaining_debt);

            boost::property_tree::ptree json;
            json.put("user_id", user_id);
            json.put("amount", amount);
            json.put("account_id", account_id);
            json.put("type_message", "write_off_money");
            json.put("message", "Monthly payment");

            std::ostringstream oss;
            boost::property_tree::write_json(oss, json, false);
            async_publish(oss.str());
        }

        PQclear(db_result);

    } catch (const std::exception& e) {
        std::cerr << "Periodic processing error: " << e.what() << std::endl;
    }
    int period = std::stoi(config_.at("ADD_PAY_TIMEOUT_SEC"));
    // Перезапуск таймера
    periodic_timer_.expires_after(boost::asio::chrono::seconds(period));
    start_periodic_timer();
}

