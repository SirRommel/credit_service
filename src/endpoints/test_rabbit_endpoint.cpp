#include "test_rabbit_endpoint.h"
#include "../tools/json_tools.h"
#include "../errors/http_errors.h"
#include "errors/http_errors.h"
#include "tools/json_tools.h"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

namespace app {
    namespace endpoints {

        boost::beast::http::response<boost::beast::http::string_body>
        TestRabbitEndpoint::handle(const boost::beast::http::request<boost::beast::http::string_body>& req) {
            if (req.method() != boost::beast::http::verb::post) {
                return errors::method_not_allowed(req);
            }

            try {
                std::string test_message = R"({"test": "Hello RabbitMQ!"})";
                rabbitmq_.async_publish(test_message);

                boost::property_tree::ptree root;
                root.put("status", "success");
                root.put("message", "Message sent to RabbitMQ");

                std::ostringstream oss;
                boost::property_tree::write_json(oss, root);

                boost::beast::http::response<boost::beast::http::string_body> res;
                res.result(boost::beast::http::status::ok);
                res.set(boost::beast::http::field::content_type, "application/json");
                res.body() = oss.str();
                res.prepare_payload(); // Важно для правильного вычисления Content-Length

                return res;

            } catch (const std::exception& e) {
                return errors::create_error_response(
                    boost::beast::http::status::internal_server_error,
                    e.what(),
                    req
                );
            }
        }

    } // namespace endpoints
} // namespace app