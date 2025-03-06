#ifndef APP_ENDPOINTS_TIME_ENDPOINT_H
#define APP_ENDPOINTS_TIME_ENDPOINT_H

#include "endpoint.h"
#include <chrono>
#include <iomanip>
#include <sstream>

namespace app {
    namespace endpoints {

        class TimeEndpoint : public Endpoint {
        public:
            boost::beast::http::response<boost::beast::http::string_body>
            handle(const boost::beast::http::request<boost::beast::http::string_body>& req) override;

            const std::unordered_set<boost::beast::http::verb>&
            allowed_methods() const override {
                static const std::unordered_set<boost::beast::http::verb> methods = {
                    boost::beast::http::verb::get
                };
                return methods;
            }
        };

    } // namespace endpoints
} // namespace app

#endif // APP_ENDPOINTS_TIME_ENDPOINT_H