#include "app.h"
#include "src/db/database_manager.h"
#include "src/tools/utils.h"
#include <iostream>

#include "db/database_initializer.h"
#include "models/db_models/tariff_model.h"
#include "models/db_models/credit_history_model.h"
#include "models/db_models/credit_model.h"
#include <csignal>
#include <atomic>

#include "models/db_models/credit_payment_model.h"
std::atomic<bool> running{true};

std::mutex mtx;
std::condition_variable cv;

void signal_handler(int) {
    running = false;
    cv.notify_one();
}

int main() {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    try {
        auto config = read_env_file(".env");

        // Регистрация моделей
        auto tariff_model = std::make_shared<TariffModel>();
        auto credit_history_model = std::make_shared<CreditHistoryModel>();
        auto credit_model = std::make_shared<CreditModel>();
        auto credit_payment_model = std::make_shared<CreditPaymentModel>();
        DatabaseInitializer::instance().register_model(tariff_model);
        DatabaseInitializer::instance().register_model(credit_history_model);
        DatabaseInitializer::instance().register_model(credit_model);
        DatabaseInitializer::instance().register_model(credit_payment_model);

        db::DatabaseManager db(config);
        RabbitMQManager rabbitmq(config, db);
        db.start();


        App app(config, db, rabbitmq);
        app.run();

        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [] { return !running.load(); });

        // app.stop();
        // rabbitmq.stop();
        // db.stop();

    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
