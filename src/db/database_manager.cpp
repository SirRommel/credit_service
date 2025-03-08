#include "database_manager.h"
#include <iostream>
#include <map>
#include <thread>

#include "database_initializer.h"

namespace db {

    DatabaseManager::DatabaseManager(const std::map<std::string, std::string>& config)
        : config_(config),
          ioc_(),
          work_(ioc_.get_executor()),
          conn_(nullptr),
          running_(true) {
        connect();
        if (conn_ && PQstatus(conn_) == CONNECTION_OK) {
            initialize_tables(); // Инициализация таблиц
        }
    }

    void DatabaseManager::start() {
        thread_ = std::thread([this]() {
            connect();
            ioc_.run();
        });
    }

    DatabaseManager::~DatabaseManager() {
        stop(); // Правильное завершение работы
    }

    void DatabaseManager::stop() {
        running_ = false;
        work_.reset(); // Сброс work_guard
        ioc_.stop();
        if (conn_) PQfinish(conn_);
        if (thread_.joinable()) thread_.join();
    }

    void DatabaseManager::connect() {
        std::string conn_str = "dbname=" + config_.at("POSTGRES_DB") +
            " user=" + config_.at("POSTGRES_USER") +
            " password=" + config_.at("POSTGRES_PASSWORD") +
            " host=" + config_.at("POSTGRES_HOST") +
            " port=" + config_.at("POSTGRES_PORT");

        conn_ = PQconnectdb(conn_str.c_str());
        if (PQstatus(conn_) != CONNECTION_OK) {
            std::cerr << "Connection failed: " << PQerrorMessage(conn_) << std::endl;
            PQfinish(conn_);
            conn_ = nullptr;
            stop();
            return;
        }
        else {
            std::cout << "Connected to database " << config_.at("POSTGRES_DB") << std::endl;
        }
    }


    void DatabaseManager::async_query_params(
        const std::string& query,
        const char* const* paramValues,
        int nParams,
        std::function<void(PGresult*)> callback)
    {
        std::cout << query << std::endl;
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (!running_) {
            if (callback) callback(nullptr);
            return;
        }

        query_queue_.push({
            QueryItem::Type::PARAMETERIZED,
            query,
            paramValues,
            nParams,
            callback
        });

        ioc_.post([this]() { process_queue(); });
    }

    void DatabaseManager::process_queue() {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        if (!running_) return;

        if (!conn_ || PQstatus(conn_) != CONNECTION_OK) {
            connect();
            if (!conn_ || PQstatus(conn_) != CONNECTION_OK) {
                while (!query_queue_.empty()) {
                    auto& item = query_queue_.front();
                    if (item.callback) item.callback(nullptr);
                    query_queue_.pop();
                }
                return;
            }
        }

        while (!query_queue_.empty()) {
            QueryItem item = std::move(query_queue_.front());
            query_queue_.pop();
            lock.unlock();

            PGresult* res = nullptr;

            if (item.type == QueryItem::Type::SIMPLE) {
                res = PQexec(conn_, item.query.c_str());
            } else {
                res = PQexecParams(conn_,
                    item.query.c_str(),
                    item.nParams,
                    nullptr,
                    item.paramValues,
                    nullptr,
                    nullptr,
                    0);
            }
            if (res) {
                std::cout << "Query status: " << PQresStatus(PQresultStatus(res)) << std::endl;
                if (PQresultStatus(res) != PGRES_COMMAND_OK && PQresultStatus(res) != PGRES_TUPLES_OK) {
                    std::cerr << "Query error: " << PQerrorMessage(conn_) << std::endl;
                }
            } else {
                std::cerr << "Query failed: no result" << std::endl;
            }

            if (PQstatus(conn_) != CONNECTION_OK) {
                PQclear(res);
                res = nullptr;
            }

            if (item.callback) {
                item.callback(res);
            } else {
                PQclear(res);
            }

            lock.lock();
        }
    }

    void DatabaseManager::initialize_tables() {
        if (!conn_ || PQstatus(conn_) != CONNECTION_OK) {
            std::cerr << "Cannot initialize tables: no database connection" << std::endl;
            return;
        }

        // Добавляем расширение uuid-ossp
        std::string create_extension = "CREATE EXTENSION IF NOT EXISTS \"uuid-ossp\"";
        PGresult* ext_res = PQexec(conn_, create_extension.c_str());
        if (PQresultStatus(ext_res) != PGRES_COMMAND_OK) {
            std::cerr << "Failed to create extension: " << PQerrorMessage(conn_) << std::endl;
        }
        PQclear(ext_res);

        auto& initializer = DatabaseInitializer::instance();
        for (auto& model : initializer.get_models()) {
            std::string query = model->get_create_table_query();
            PGresult* res = PQexec(conn_, query.c_str());

            if (PQresultStatus(res) != PGRES_COMMAND_OK) {
                std::cerr << "Failed to create table " << model->get_table_name()
                          << ": " << PQerrorMessage(conn_) << std::endl;
            } else {
                std::cout << "Table " << model->get_table_name()
                          << " initialized successfully" << std::endl;
            }
            PQclear(res);
        }
    }

    std::string DatabaseManager::get_last_error() const {
        if (conn_) {
            return PQerrorMessage(conn_);
        }
        return "No connection";
    }



} // namespace db