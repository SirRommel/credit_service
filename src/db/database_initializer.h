#ifndef DATABASE_INITIALIZER_H
#define DATABASE_INITIALIZER_H

#include <vector>
#include <memory>
#include "../models/db_models/base_db_model.h"

class DatabaseInitializer {
public:
    static DatabaseInitializer& instance() {
        static DatabaseInitializer instance;
        return instance;
    }

    void register_model(std::shared_ptr<BaseModel> model) {
        models_.push_back(model);
    }

    const std::vector<std::shared_ptr<BaseModel>>& get_models() const {
        return models_;
    }

private:
    DatabaseInitializer() = default;
    std::vector<std::shared_ptr<BaseModel>> models_;
};

#endif //DATABASE_INITIALIZER_H