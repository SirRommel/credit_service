//
// Created by rommel on 27.04.2025.
//

#ifndef CREDIT_SERVICE_METRICS_ENDPOINT_H
#define CREDIT_SERVICE_METRICS_ENDPOINT_H

#include <boost/beast/http/string_body.hpp>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <prometheus/registry.h>
#include "endpoint.h"

class MetricsEndpoint : public app::endpoints::Endpoint {
public:
    explicit MetricsEndpoint(std::shared_ptr<prometheus::Registry> registry)
            : registry_(std::move(registry)) {}

    boost::beast::http::response<boost::beast::http::string_body> handle(
            const boost::beast::http::request<boost::beast::http::string_body>& req) override;

    const std::unordered_set<boost::beast::http::verb>&
    allowed_methods() const override {
        static const std::unordered_set<boost::beast::http::verb> methods{
                boost::beast::http::verb::get
        };
        return methods;
    }

private:
    std::shared_ptr<prometheus::Registry> registry_;
};
#endif //CREDIT_SERVICE_METRICS_ENDPOINT_H
