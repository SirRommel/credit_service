// CreditEndpoint.h
#ifndef CREDIT_ENDPOINT_H
#define CREDIT_ENDPOINT_H

#include "endpoint.h"
#include "../db/database_manager.h"
#include "../rabbit_mq/rabbitmq_manager.h"
#include <boost/uuid/uuid.hpp>

class CreditEndpoint : public app::endpoints::Endpoint {
public:
    CreditEndpoint(db::DatabaseManager& db, RabbitMQManager& rabbitmq)
        : db_(db), rabbitmq_(rabbitmq) {}

    boost::beast::http::response<boost::beast::http::string_body>
    handle(const boost::beast::http::request<boost::beast::http::string_body>& req) override;

    const std::unordered_set<boost::beast::http::verb>&
    allowed_methods() const override {
        static const std::unordered_set<boost::beast::http::verb> methods = {
            boost::beast::http::verb::post
        };
        return methods;
    }

private:
    db::DatabaseManager& db_;
    RabbitMQManager& rabbitmq_;

    bool validate_uuid(const std::string& uuid_str) const;
    std::string escape_sql_string(const std::string& input) const;
};

#endif