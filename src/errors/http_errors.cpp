#include "http_errors.h"

#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>

namespace app {
    namespace errors {

        template <typename Body>
        boost::beast::http::response<boost::beast::http::string_body>
        create_error_response(
            boost::beast::http::status status,
            const std::string& message,
            const boost::beast::http::request<Body>& req)
        {
            boost::beast::http::response<boost::beast::http::string_body> res{status, req.version()};
            res.set(boost::beast::http::field::content_type, "text/plain");
            res.body() = message;
            res.prepare_payload();
            return res;
        }

        boost::beast::http::response<boost::beast::http::string_body>
        not_found(const boost::beast::http::request<boost::beast::http::string_body>& req) {
            return create_error_response(
                boost::beast::http::status::not_found,
                "404 Not Found",
                req);
        }

        boost::beast::http::response<boost::beast::http::string_body>
        method_not_allowed(const boost::beast::http::request<boost::beast::http::string_body>& req) {
            return create_error_response(
                boost::beast::http::status::method_not_allowed,
                "Method Not Allowed",
                req);
        }

    } // namespace errors
} // namespace app