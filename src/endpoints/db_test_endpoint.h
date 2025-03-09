// endpoints/db_test_endpoint.h
#include "../db/database_manager.h"

class DbTestEndpoint : public app::endpoints::Endpoint {
public:
    DbTestEndpoint(db::DatabaseManager& db) : db_(db) {}

    http::response<http::string_body>
    handle(const http::request<http::string_body>& req) {
        std::promise<PGresult*> promise;
        auto future = promise.get_future();
        const char* insertParams[] = {};
        db_.async_query_params(
            "SELECT version();",
            insertParams,
            2,
            [&](PGresult* res) { promise.set_value(res); }
        );

        PGresult* res = future.get();

        if (!res || PQresultStatus(res) != PGRES_TUPLES_OK) {
            std::string error = res ? PQresultErrorMessage(res) : "Unknown database error";
            PQclear(res);
            throw std::runtime_error(error);
        }

        int rows = PQntuples(res);
        int cols = PQnfields(res);

        if (rows < 1 || cols < 1) {
            PQclear(res);
            throw std::runtime_error("Empty result set");
        }

        std::string version = PQgetvalue(res, 0, 0);
        PQclear(res);

        http::response<http::string_body> response{http::status::ok, req.version()};
        response.body() = "Database version: " + version;
        return response;
    }

    const std::unordered_set<http::verb>& allowed_methods() const override {
        static const std::unordered_set<http::verb> methods = {http::verb::get};
        return methods;
    }

private:
    db::DatabaseManager& db_;
};