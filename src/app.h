#ifndef APP_H
#define APP_H

#include <map>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <thread>
#include <memory>

#include "endpoint_trie.h"
#include "endpoints/endpoint.h"
#include "tools/utils.h"
#include "db/database_manager.h"
#include "rabbit_mq/rabbitmq_manager.h"


namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

class App {
public:
    App(const std::map<std::string, std::string>& config,
        db::DatabaseManager& db,
        RabbitMQManager& rabbitmq);
    ~App();
    void run();
    void stop();

private:
    class EndpointTrie {
    public:
        void insert(const std::string& path, std::shared_ptr<app::endpoints::Endpoint> endpoint);
        std::shared_ptr<app::endpoints::Endpoint> find(const std::string& target);
        mutable std::unordered_map<std::string, std::shared_ptr<app::endpoints::Endpoint>> cache_;
        static constexpr size_t CACHE_SIZE = 1000;

    private:
        struct TrieNode {
            std::unordered_map<char, std::unique_ptr<TrieNode>> children;
            std::shared_ptr<app::endpoints::Endpoint> endpoint = nullptr;
        };

        TrieNode root;
    };

    void accept();
    void handle_request(tcp::socket socket, beast::flat_buffer buffer);
    void register_endpoints();
    std::unique_ptr<app::endpoints::Endpoint>& find_endpoint(const std::string& target);

    std::unordered_map<std::string, std::unique_ptr<app::endpoints::Endpoint>> fixed_endpoints_;
    EndpointTrie endpointTrie;
    db::DatabaseManager& db_;
    RabbitMQManager& rabbitmq_;
    std::unordered_map<std::string, std::unique_ptr<app::endpoints::Endpoint>> endpoints_;
    net::io_context ioc_;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard_;
    tcp::acceptor acceptor_;
    std::thread thread_;
    std::vector<std::thread> threads_;
    std::map<std::string, std::string> config_;

};
#endif