#ifndef APP_H
#define APP_H

#include <map>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <thread>
#include <memory>
#include "endpoints/endpoint.h"
#include "tools/utils.h"
#include "db/database_manager.h"


namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

class App {
public:
    App(const std::map<std::string, std::string>& config, db::DatabaseManager& db);
    void run();
    void stop();

private:
    void accept();
    void handle_request(tcp::socket socket, beast::flat_buffer buffer);
    void register_endpoints();

    db::DatabaseManager& db_;
    std::unordered_map<std::string, std::unique_ptr<app::endpoints::Endpoint>> endpoints_;
    net::io_context ioc_;
    tcp::acceptor acceptor_;
    std::thread thread_;
    std::map<std::string, std::string> config_;
};
#endif