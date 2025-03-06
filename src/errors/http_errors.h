#ifndef APP_ERRORS_HTTP_ERRORS_H
#define APP_ERRORS_HTTP_ERRORS_H

#include <boost/beast/http.hpp>

namespace app {
    namespace errors {

        template <typename Body>
        boost::beast::http::response<boost::beast::http::string_body>
        create_error_response(
            boost::beast::http::status status,
            const std::string& message,
            const boost::beast::http::request<Body>& req);

        boost::beast::http::response<boost::beast::http::string_body>
        not_found(const boost::beast::http::request<boost::beast::http::string_body>& req);

        boost::beast::http::response<boost::beast::http::string_body>
        method_not_allowed(const boost::beast::http::request<boost::beast::http::string_body>& req);

    } // namespace errors
} // namespace app

#endif // APP_ERRORS_HTTP_ERRORS_H