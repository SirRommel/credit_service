// CreditEndpoint.cpp
#include "credit_history.h"

#include "../tools/json_tools.h"
#include "../errors/http_errors.h"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/random_generator.hpp>
#include <regex>
#include <chrono>

#include "tools/utils.h"

using namespace std::literals;

// UUID validation regex
static const std::regex uuid_regex(
    "^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$"
);

bool CreditHistoryEndpoint::validate_uuid(const std::string& uuid_str) const {
    return std::regex_match(uuid_str, uuid_regex);
}

std::string CreditHistoryEndpoint::escape_sql_string(const std::string& input) const {
    if (!db_.get_connection()) {
        throw std::runtime_error("No database connection");
    }
    char* escaped = PQescapeLiteral(db_.get_connection(), input.c_str(), input.size());
    if (!escaped) {
        throw std::runtime_error("Failed to escape string");
    }
    std::string result(escaped);
    PQfreemem(escaped);
    return result;
}

boost::beast::http::response<boost::beast::http::string_body>
CreditHistoryEndpoint::handle(const boost::beast::http::request<boost::beast::http::string_body>& req) {
    try {

        if (req.method() == boost::beast::http::verb::get) {
            std::string id = extract_id_from_path(std::string(req.target()));
            std::string result = get_credit_history(id);
            boost::beast::http::response<boost::beast::http::string_body> res{boost::beast::http::status::ok, req.version()};
            res.set(boost::beast::http::field::content_type, "application/json");
            res.body() = result;
            return res;
        }


    } catch (const std::invalid_argument& e) {
        return app::errors::create_error_response(
            boost::beast::http::status::bad_request,
            e.what(),
            req);
    } catch (const std::exception& e) {
        return app::errors::create_error_response(
            boost::beast::http::status::internal_server_error,
            e.what(),
            req);
    }
}


std::string CreditHistoryEndpoint::get_credit_history(std::string id) {
    std::promise<PGresult*> promise;
    auto future = promise.get_future();
    const char* params[] = {id.c_str()};
    db_.async_query_params(
        "SELECT * FROM credit_payments WHERE user_id = ($1)",
        params,
        1,
        [&](PGresult* res) { promise.set_value(res); }
    );

    PGresult* res = future.get();
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        throw std::runtime_error("Failed to fetch credits");
    }

    boost::property_tree::ptree pt;
    boost::property_tree::ptree credits;

    int rows = PQntuples(res);
    for (int i = 0; i < rows; ++i) {
        boost::property_tree::ptree credit;
        credit.put("id", PQgetvalue(res, i, 0));
        credit.put("user_id", PQgetvalue(res, i, 1));
        credit.put("summ", PQgetvalue(res, i, 2));
        std::string status = PQgetvalue(res, i, 3);
        if (status == "t") {
            credit.put("status", true);
        } else if (status == "f") {
            credit.put("status", false);
        } else {
            credit.put("status", status); // Если статус не "t" или "f", оставляем как есть
        }
        credit.put("payment_date", PQgetvalue(res, i, 4));
        credits.push_back(std::make_pair("", credit));
    }

    PQclear(res);
    pt.add_child("credit_payments", credits);

    std::ostringstream oss;
    boost::property_tree::write_json(oss, pt, false);
    std::string jsonStr = oss.str();
    std::cout << jsonStr << std::endl;
    std::regex summ_regex("\"summ\":\\s*\"([0-9\\.]+)\"");
    jsonStr = std::regex_replace(jsonStr, summ_regex, "\"summ\": $1");

    std::regex bool_regex("\"status\":\\s*\"(true|false)\"");
    jsonStr = std::regex_replace(jsonStr, bool_regex, "\"status\": $1");

    return jsonStr;

}