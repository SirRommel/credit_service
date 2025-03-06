#ifndef CREDIT_HISTORY_MODEL_H
#define CREDIT_HISTORY_MODEL_H
#include "base_db_model.h"

class CreditHistoryModel : public BaseModel {
public:
    std::string get_table_name() const override {
        return "credit_history";
    }

    std::vector<std::string> get_columns() const override {
        return {
            "user_id UUID PRIMARY KEY",
            "rating NUMERIC(5,2) CHECK (rating BETWEEN 0 AND 100)"
        };
    }
};
#endif //CREDIT_HISTORY_MODEL_H
