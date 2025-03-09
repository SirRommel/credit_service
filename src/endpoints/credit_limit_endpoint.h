// CreditLimitEndpoint.h
#include "endpoint.h"
#include "../db/database_manager.h"

class CreditLimitEndpoint : public app::endpoints::Endpoint {
public:
    CreditLimitEndpoint(db::DatabaseManager& db) : db_(db) {}

    boost::beast::http::response<boost::beast::http::string_body> handle(const boost::beast::http::request<boost::beast::http::string_body>& req) override;

    const std::unordered_set<boost::beast::http::verb>& allowed_methods() const override {
        static const std::unordered_set<boost::beast::http::verb> methods = {boost::beast::http::verb::get};
        return methods;
    }

private:
    db::DatabaseManager& db_;
};