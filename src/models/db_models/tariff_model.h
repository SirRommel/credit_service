#ifndef TARIFF_MODEL_H
#define TARIFF_MODEL_H
#include "base_db_model.h"

class TariffModel : public BaseModel {
public:
    std::string get_table_name() const override {
        return "tariffs";
    }

    std::vector<std::string> get_columns() const override {
        return {
            "id SERIAL PRIMARY KEY",
            "employee_id INTEGER NOT NULL",
            "name VARCHAR(255) NOT NULL",
            "interest_rate NUMERIC(5,2) CHECK (interest_rate BETWEEN 0 AND 100)"
        };
    }
};
#endif //TARIFF_MODEL_H
