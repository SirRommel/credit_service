
#include "metrics_endpoint.h"
#include "errors/http_errors.h"
#include <boost/beast/http/string_body.hpp>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <prometheus/registry.h>
#include <prometheus/counter.h>
#include <prometheus/exposer.h>
#include <prometheus/gauge.h>
#include <prometheus/serializer.h>
#include <prometheus/summary.h>
#include <prometheus/text_serializer.h>
boost::beast::http::response<boost::beast::http::string_body> MetricsEndpoint::handle(
        const boost::beast::http::request<boost::beast::http::string_body>& req) {

    if (req.method() != boost::beast::http::verb::get) {
        return app::errors::method_not_allowed(req);
    }

    std::ostringstream oss;
    prometheus::TextSerializer serializer;
    serializer.Serialize(oss, registry_->Collect());

    boost::beast::http::response<boost::beast::http::string_body> res{boost::beast::http::status::ok, req.version()};
    res.set(boost::beast::http::field::content_type, "text/plain; version=0.0.4");
    res.body() = oss.str();
    return res;
}