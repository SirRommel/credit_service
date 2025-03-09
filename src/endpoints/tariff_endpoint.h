#ifndef TARIFF_ENDPOINT_H
#define TARIFF_ENDPOINT_H

#include "endpoint.h"
#include "../db/database_manager.h"
#include <boost/beast/http.hpp>
#include <string>
#include <boost/beast.hpp>
#include <string_view>

namespace app {
    namespace endpoints {
        namespace http = boost::beast::http;
        class TariffEndpoint : public Endpoint {
        public:
            TariffEndpoint(db::DatabaseManager& db) : db_(db) {}

            boost::beast::http::response<http::string_body>
            handle(const http::request<http::string_body>& req) override;

            const std::unordered_set<http::verb>& allowed_methods() const override {
                static const std::unordered_set<http::verb> methods = {
                    http::verb::get,
                    http::verb::post,
                    http::verb::put,
                    http::verb::delete_
                };
                return methods;
            }

        private:
            db::DatabaseManager& db_;

            std::string create_tariff(const std::string& body);
            std::string get_all_tariffs(std::string id);
            std::string update_tariff(std::string id, const std::string& body);
            std::string delete_tariff(std::string id);
        };

    } // namespace endpoints
} // namespace app

#endif // TARIFF_ENDPOINT_H