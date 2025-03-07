#ifndef TEST_RABBIT_ENDPOINT_H
#define TEST_RABBIT_ENDPOINT_H

#include "endpoint.h"
#include "../rabbit_mq/rabbitmq_manager.h"

namespace app {
    namespace endpoints {

        class TestRabbitEndpoint : public Endpoint {
        public:
            TestRabbitEndpoint(RabbitMQManager& rabbitmq)
                : rabbitmq_(rabbitmq) {}

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
            RabbitMQManager& rabbitmq_;
        };

    } // namespace endpoints
} // namespace app

#endif // TEST_RABBIT_ENDPOINT_H