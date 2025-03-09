#include "app.h"
#include "src/db/database_manager.h"
#include "src/tools/utils.h"
#include <iostream>

#include "db/database_initializer.h"
#include "models/db_models/tariff_model.h"
#include "models/db_models/credit_history_model.h"
#include "models/db_models/credit_model.h"

int main() {
    try {
        auto config = read_env_file(".env");

        // Регистрация моделей
        auto tariff_model = std::make_shared<TariffModel>();
        auto credit_history_model = std::make_shared<CreditHistoryModel>();
        auto credit_model = std::make_shared<CreditModel>();
        DatabaseInitializer::instance().register_model(tariff_model);
        DatabaseInitializer::instance().register_model(credit_history_model);
        DatabaseInitializer::instance().register_model(credit_model);

        db::DatabaseManager db(config);
        RabbitMQManager rabbitmq(config, db);
        db.start();


        App app(config, db, rabbitmq);
        app.run();

        std::cin.get();

        app.stop();
        db.stop();
        rabbitmq.stop();
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
