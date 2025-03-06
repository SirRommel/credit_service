#ifndef APP_ENDPOINTS_ENDPOINT_H
#define APP_ENDPOINTS_ENDPOINT_H

#include <boost/beast/http.hpp>
#include <unordered_set>

namespace app {
    namespace endpoints {

        class Endpoint {
        public:
            virtual ~Endpoint() = default;

            virtual boost::beast::http::response<boost::beast::http::string_body>
            handle(const boost::beast::http::request<boost::beast::http::string_body>& req) = 0;

            virtual const std::unordered_set<boost::beast::http::verb>&
            allowed_methods() const = 0;
        };

    } // namespace endpoints
} // namespace app

#endif // APP_ENDPOINTS_ENDPOINT_H