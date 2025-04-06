
#ifndef CREDIT_PAYMENT_MODEL_H
#define CREDIT_PAYMENT_MODEL_H

#include "base_db_model.h"

class CreditPaymentModel : public BaseModel {
public:
    std::string get_table_name() const override {
        return "credit_payments";
    }

    std::vector<std::string> get_columns() const override {
        return {
            "id UUID PRIMARY KEY DEFAULT uuid_generate_v4()",
            "user_id UUID NOT NULL",
            "summ NUMERIC(15,2) NOT NULL CHECK (summ > 0)",
            "status BOOLEAN",
            "payment_date TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP"
        };
    }
};

#endif // CREDIT_PAYMENT_MODEL_H