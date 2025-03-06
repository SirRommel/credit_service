#ifndef DB_DATABASE_MANAGER_H
#define DB_DATABASE_MANAGER_H

#include <boost/asio.hpp>
#include <libpq-fe.h>
#include <memory>
#include <string>
#include <functional>
#include <map>
#include <queue>

namespace db {

    class DatabaseManager {
    public:
        DatabaseManager(const std::map<std::string, std::string>& config);
        ~DatabaseManager();

        void start();
        void stop();
        std::string get_last_error() const;
        void initialize_tables();

        // Асинхронное выполнение запроса
        void async_query(
            const std::string& query,
            std::function<void(PGresult*)> callback);

    private:
        void connect();
        void process_queue();
        void worker_thread();

        std::map<std::string, std::string> config_;
        boost::asio::io_context ioc_;
        std::thread thread_;
        PGconn* conn_;
        std::queue<std::pair<std::string, std::function<void(PGresult*)>>> query_queue_;
        boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_;
        std::mutex queue_mutex_;
        bool running_;
    };

} // namespace db
#endif