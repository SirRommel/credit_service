#include "time_endpoint.h"
#include <boost/beast/http.hpp>

namespace app {
    namespace endpoints {
        boost::beast::http::response<boost::beast::http::string_body>
        TimeEndpoint::handle(const boost::beast::http::request<boost::beast::http::string_body>& req) {
            auto now = std::chrono::system_clock::now();
            std::time_t now_time = std::chrono::system_clock::to_time_t(now);
            std::stringstream ss;
            ss << std::put_time(std::localtime(&now_time), "%Y-%m-%d %X");

            boost::beast::http::response<boost::beast::http::string_body> res{boost::beast::http::status::ok, req.version()};
            res.set(boost::beast::http::field::content_type, "text/plain");
            res.body() = "Current time: " + ss.str();
            return res;
        }

    } // namespace endpoints
} // namespace app