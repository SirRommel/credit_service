#include "app.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "endpoints/time_endpoint.h"
#include "errors/http_errors.h"
#include "tools/utils.h"
#include "db/database_manager.h"
#include "endpoints/db_test_endpoint.h"
#include "endpoints/tariff_endpoint.h"
#include "endpoints/credit_limit_endpoint.h"
#include "endpoints/test_rabbit_endpoint.h"
#include "endpoints/credit_endpoint.h"


App::App(const std::map<std::string, std::string>& config, db::DatabaseManager& db, RabbitMQManager& rabbitmq)
    : ioc_(),
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
        throw; // Перебрасываем исключение
    }


    register_endpoints();
    accept();
}

void App::run() {
    std::cout << "Server started on "
              << config_.at("HOST") << ":"
              << config_.at("PORT") << std::endl;
    thread_ = std::thread([this]() { ioc_.run(); });
}

void App::stop() {
    ioc_.stop();
    if (thread_.joinable()) {
        thread_.join();
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
    endpoints_["/time"] = std::make_unique<app::endpoints::TimeEndpoint>();
    endpoints_["/db-test"] = std::make_unique<DbTestEndpoint>(db_);
    endpoints_["/tariffs"] = std::make_unique<app::endpoints::TariffEndpoint>(db_);
    endpoints_["/credit-limit"] = std::make_unique<CreditLimitEndpoint>(db_);
    endpoints_["/test-rabbit"] = std::make_unique<app::endpoints::TestRabbitEndpoint>(rabbitmq_);
    endpoints_["/credit"] = std::make_unique<CreditEndpoint>(db_, rabbitmq_);
}

void App::handle_request(tcp::socket socket, beast::flat_buffer buffer) {
    http::request<http::string_body> req;
    http::read(socket, buffer, req);

    // Логирование запроса
    std::cout << "Received " << req.method()
              << " request for " << req.target() << std::endl;

    http::response<http::string_body> res;

    try {
        auto target = std::string(req.target());
        auto it = endpoints_.find(target);

        if (it == endpoints_.end() && target.find("/credit/") == 0) {
            it = endpoints_.find("/credit");
        }

        if (it == endpoints_.end() && target.find("/credit-limit/") == 0) {
            it = endpoints_.find("/credit-limit");
        }

        if (it == endpoints_.end() && target.find("/tariffs/") == 0) {
            it = endpoints_.find("/tariffs");
        }

        if (it == endpoints_.end() && target.find("/test-rabbit/") == 0) {
            it = endpoints_.find("/test-rabbit");
        }

        if (it != endpoints_.end()) {
            if (it->second->allowed_methods().count(req.method()) == 0) {
                res = app::errors::method_not_allowed(req);
            } else {
                res = it->second->handle(req);
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