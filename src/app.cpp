#include "app.h"
#include "../thread_config.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <boost/regex.hpp>
#include "endpoints/time_endpoint.h"
#include "errors/http_errors.h"
#include "tools/utils.h"
#include "db/database_manager.h"
#include "endpoints/db_test_endpoint.h"
#include "endpoints/tariff_endpoint.h"
#include "endpoints/credit_limit_endpoint.h"
#include "endpoints/test_rabbit_endpoint.h"
#include "endpoints/credit_endpoint.h"
#include "endpoints/credit_history.h"
#include "endpoints/credits_endpoint.h"
#include "endpoints/metrics_endpoint.h"
#include <prometheus/counter.h>
#include <prometheus/exposer.h>
#include <prometheus/gauge.h>
#include <prometheus/registry.h>
#include <prometheus/serializer.h>
#include <prometheus/summary.h>
#include <prometheus/text_serializer.h>
#include <random>
#include <ctime>


struct RequestTracker {
    prometheus::Gauge& active_requests;
    prometheus::Summary& duration;
    prometheus::Counter& errors;
    std::chrono::steady_clock::time_point start;
    bool success = false;

    RequestTracker(prometheus::Gauge& active,
                   prometheus::Summary& dur,
                   prometheus::Counter& err)
            : active_requests(active), duration(dur), errors(err),
              start(std::chrono::steady_clock::now()) {
        active_requests.Increment();
    }

    ~RequestTracker() {
        active_requests.Decrement();
        auto dur = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - start).count();
        duration.Observe(dur);
        if (!success) errors.Increment();
    }
};






void App::EndpointTrie::insert(const std::string& path, std::shared_ptr<app::endpoints::Endpoint> endpoint) {
    TrieNode* node = &root;
    for (char c : path) {
        if (!node->children.count(c)) {
            node->children[c] = std::make_unique<TrieNode>();
        }
        node = node->children[c].get();
    }
    node->endpoint = endpoint;
}
std::shared_ptr<app::endpoints::Endpoint> App::EndpointTrie::find(const std::string& target) {
    TrieNode* node = &root;
    std::shared_ptr<app::endpoints::Endpoint> last_match = nullptr;

    for (char c : target) {
        if (!node->children.count(c)) break;
        node = node->children[c].get();
        if (node->endpoint) last_match = node->endpoint;
    }

    return last_match;
}

App::App(const std::map<std::string, std::string>& config, db::DatabaseManager& db, RabbitMQManager& rabbitmq)
    : ioc_(),
      work_guard_(boost::asio::make_work_guard(ioc_)),
      acceptor_(ioc_),
      config_(config),
      db_(db),
      rabbitmq_(rabbitmq)
{

    std::string host = "0.0.0.0";
    unsigned short port = 8080;

    auto host_it = config_.find("HOST");
    if (host_it != config_.end()) {
        host = host_it->second;
    }

    auto port_it = config_.find("PORT");
    if (port_it != config_.end()) {
        port = static_cast<unsigned short>(std::stoi(port_it->second));
    }

    try {
        boost::asio::ip::tcp::endpoint endpoint;
        if (host == "0.0.0.0") {
            endpoint = tcp::endpoint(tcp::v4(), port);
        } else {
            auto ip_address = boost::asio::ip::make_address(host);
            endpoint = tcp::endpoint(ip_address, port);
        }

        acceptor_.open(endpoint.protocol());
        acceptor_.bind(endpoint);
        acceptor_.listen();
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize server on "
                  << host << ":" << port << " - " << e.what() << std::endl;
        throw;
    }

    registry_ = std::make_shared<prometheus::Registry>();

    auto& active_requests_family = prometheus::BuildGauge()
            .Name("http_server_requests_active_seconds_count")
            .Help("Active requests in progress")
            .Register(*registry_);
    active_requests_ = &active_requests_family.Add({});

    auto& errors_family = prometheus::BuildCounter()
            .Name("http_request_errors_total")
            .Help("Total number of request errors")
            .Register(*registry_);
    request_errors_ = &errors_family.Add({});

    auto& duration_family = prometheus::BuildSummary()
            .Name("http_request_duration_seconds")
            .Help("Request duration summary")
            .Register(*registry_);


    request_duration_ = &duration_family.Add(
            {},
            prometheus::Summary::Quantiles{{0.5, 0.05}, {0.9, 0.01}, {0.99, 0.001}},
            std::chrono::seconds(60),
            5
    );

    // В конструкторе App:
    endpointTrie.insert(
            "/metrics",
            std::static_pointer_cast<app::endpoints::Endpoint>(
                    std::make_shared<MetricsEndpoint>(registry_)
            )
    );


    register_endpoints();
    accept();
}

App::~App() {
    stop();
}

void App::run() {
    std::cout << "Server started on "
              << config_.at("HOST") << ":"
              << config_.at("PORT") << std::endl;
    for (int i = 0; i < app_thread_count; ++i) {
        threads_.emplace_back([this]() { ioc_.run(); });
    }
}

void App::stop() {
    try {
        work_guard_.reset();
        ioc_.stop();
        acceptor_.close();
        for (auto& thread : threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        std::cout << "app stopped." << std::endl;
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

void App::accept() {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket) {
            if (!ec) {
                beast::flat_buffer buffer;
                handle_request(std::move(socket), std::move(buffer));
            }
            accept();
        });
}


void App::register_endpoints() {
    // при сложно роутинге наподобие bank/credit/tariffs, если есть эндпоинты в bank/credit,
    // то этот эндпоинт нужно прописать позже, чем bank/credit/tariffs, а говно, потому что порядок не зависит от инициализации
    endpoints_["/time"] = std::make_unique<app::endpoints::TimeEndpoint>();
    endpoints_["/db-test"] = std::make_unique<DbTestEndpoint>(db_);
    endpoints_["/tariffs"] = std::make_unique<app::endpoints::TariffEndpoint>(db_);
    endpoints_["/credit-limit"] = std::make_unique<CreditLimitEndpoint>(db_);
    endpoints_["/test-rabbit"] = std::make_unique<app::endpoints::TestRabbitEndpoint>(rabbitmq_);
    endpoints_["/credit"] = std::make_unique<CreditEndpoint>(db_, rabbitmq_);

    endpointTrie.insert("/time", std::make_shared<app::endpoints::TimeEndpoint>());
    endpointTrie.insert("/db-test", std::make_shared<DbTestEndpoint>(db_));
    endpointTrie.insert("/tariffs", std::make_shared<app::endpoints::TariffEndpoint>(db_));
    endpointTrie.insert("/credit-limit", std::make_shared<CreditLimitEndpoint>(db_));
    endpointTrie.insert("/test-rabbit", std::make_shared<app::endpoints::TestRabbitEndpoint>(rabbitmq_));
    endpointTrie.insert("/credit", std::make_shared<CreditEndpoint>(db_, rabbitmq_));
    endpointTrie.insert("/credits", std::make_shared<CreditsEndpoint>(db_));
    endpointTrie.insert("/get-payment-history", std::make_shared<CreditHistoryEndpoint>(db_));

    fixed_endpoints_["/time"] = std::make_unique<app::endpoints::TimeEndpoint>();
    fixed_endpoints_["/db-test"] = std::make_unique<DbTestEndpoint>(db_);
    fixed_endpoints_["/tariffs"] = std::make_unique<app::endpoints::TariffEndpoint>(db_);
    fixed_endpoints_["/credit-limit"] = std::make_unique<CreditLimitEndpoint>(db_);
    fixed_endpoints_["/test-rabbit"] = std::make_unique<app::endpoints::TestRabbitEndpoint>(rabbitmq_);
    fixed_endpoints_["/credit"] = std::make_unique<CreditEndpoint>(db_, rabbitmq_);
}





void App::handle_request(tcp::socket socket, beast::flat_buffer buffer) {
    RequestTracker tracker(*active_requests_,
                           *request_duration_,
                           *request_errors_);
    http::request<http::string_body> req;
    http::read(socket, buffer, req);

    std::cout << "Received " << req.method()
              << " request for " << req.target() << std::endl;

    http::response<http::string_body> res;

    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm* now_tm = std::localtime(&time);
    int current_minute = now_tm->tm_min;

    thread_local std::random_device rd;
    thread_local std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 100);
    int random_value = dis(gen);

    bool should_fail = false;
    if (current_minute % 2 == 0) {
        should_fail = (random_value <= 90);
    } else {
        should_fail = (random_value <= 50);
    }

    if (should_fail) {
        RequestTracker tracker(*active_requests_,
                               *request_duration_,
                               *request_errors_);
        http::response<http::string_body> res = app::errors::create_error_response(
                http::status::internal_server_error,
                "Simulated 500 error",
                http::request<http::string_body>()
        );
        res.prepare_payload();
        http::write(socket, res);
        socket.shutdown(tcp::socket::shutdown_send);
    }
    else {
        try {
            std::shared_ptr<app::endpoints::Endpoint> endpoint = endpointTrie.find(req.target());
            if (endpoint) {
                if (endpoint->allowed_methods().count(req.method()) == 0) {
                    res = app::errors::method_not_allowed(req);
                } else {
                    res = endpoint->handle(req);
                    tracker.success = true;
                }
            } else {
                res = app::errors::not_found(req);
            }
        } catch (...) {
            res = app::errors::create_error_response(
                    http::status::internal_server_error,
                    "Internal Server Error",
                    req);
        }

        res.prepare_payload();
        http::write(socket, res);
        socket.shutdown(tcp::socket::shutdown_send);
    }


}