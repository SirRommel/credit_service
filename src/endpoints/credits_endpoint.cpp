// CreditEndpoint.cpp
#include "credits_endpoint.h"
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

bool CreditsEndpoint::validate_uuid(const std::string& uuid_str) const {
    return std::regex_match(uuid_str, uuid_regex);
}

std::string CreditsEndpoint::escape_sql_string(const std::string& input) const {
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
CreditsEndpoint::handle(const boost::beast::http::request<boost::beast::http::string_body>& req) {
    try {
        if (req.method() == boost::beast::http::verb::get) {
            std::string id = extract_id_from_path(std::string(req.target()));
            std::string result = get_credit(id);
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

std::string CreditsEndpoint::get_credit(std::string id) {
    std::promise<PGresult*> promise;
    auto future = promise.get_future();
    const char* params[] = {id.c_str()};
    std::cout << "USERS CREDITS             F" << std::endl;
    db_.async_query_params(
            "SELECT * FROM credits WHERE user_id = ($1)",
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
        credit.put("tariff_id", PQgetvalue(res, i, 2));
        credit.put("amount", PQgetvalue(res, i, 3));
        credit.put("write_off_account", PQgetvalue(res, i, 4));
        credit.put("remaining_debt", PQgetvalue(res, i, 5));
        credits.push_back(std::make_pair("", credit));
    }

    PQclear(res);
    if (!credits.empty()) {
        pt.add_child("credits", credits);
    }
    else {
        std::ostringstream oss;
        oss << "{\"credits\": []}";
        return oss.str();
    }

    std::ostringstream oss;
    boost::property_tree::write_json(oss, pt, false);
    std::string jsonStr = oss.str();

    std::regex amount_regex("\"amount\":\\s*\"([0-9\\.]+)\"");
    jsonStr = std::regex_replace(jsonStr, amount_regex, "\"amount\": $1");

    std::regex remaining_debt_regex("\"remaining_debt\":\\s*\"([0-9\\.]+)\"");
    jsonStr = std::regex_replace(jsonStr, remaining_debt_regex, "\"remaining_debt\": $1");

    return jsonStr;

}