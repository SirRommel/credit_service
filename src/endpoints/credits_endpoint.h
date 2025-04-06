//
// Created by rommel on 06.04.2025.
//

#ifndef CREDITS_ENDPOINT_H
#define CREDITS_ENDPOINT_H
#include "endpoint.h"
#include "../db/database_manager.h"
#include "../rabbit_mq/rabbitmq_manager.h"
#include <boost/uuid/uuid.hpp>

class CreditsEndpoint : public app::endpoints::Endpoint {
public:
    CreditsEndpoint(db::DatabaseManager& db)
            : db_(db) {}

    boost::beast::http::response<boost::beast::http::string_body>
    handle(const boost::beast::http::request<boost::beast::http::string_body>& req) override;

    const std::unordered_set<boost::beast::http::verb>&
    allowed_methods() const override {
        static const std::unordered_set<boost::beast::http::verb> methods = {
                boost::beast::http::verb::post,
                boost::beast::http::verb::get
        };
        return methods;
    }

private:
    db::DatabaseManager& db_;

    std::string  get_credit(std::string id);
    bool validate_uuid(const std::string& uuid_str) const;
    std::string escape_sql_string(const std::string& input) const;
};
#endif //CREDITS_ENDPOINT_H
