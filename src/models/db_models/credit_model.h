#ifndef CREDIT_MODEL_H
#define CREDIT_MODEL_H

#include "base_db_model.h"

class CreditModel : public BaseModel {
public:
    std::string get_table_name() const override {
        return "credits";
    }

    std::vector<std::string> get_columns() const override {
        return {
            "id UUID PRIMARY KEY DEFAULT uuid_generate_v4()",
            "user_id UUID NOT NULL",
            "tariff_id UUID NOT NULL REFERENCES tariffs(id) ON DELETE CASCADE",
            "amount NUMERIC(15,2) NOT NULL CHECK (amount > 0)",
            "write_off_account_id UUID"
        };
    }
};

#endif // CREDIT_MODEL_H