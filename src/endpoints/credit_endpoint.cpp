// CreditEndpoint.cpp
#include "credit_endpoint.h"
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

bool CreditEndpoint::validate_uuid(const std::string& uuid_str) const {
    return std::regex_match(uuid_str, uuid_regex);
}

std::string CreditEndpoint::escape_sql_string(const std::string& input) const {
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
CreditEndpoint::handle(const boost::beast::http::request<boost::beast::http::string_body>& req) {
    try {
        if (req.method() == boost::beast::http::verb::post) {
            boost::beast::http::response<boost::beast::http::string_body> res = create_credit(req);
            return res;
        }

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

boost::beast::http::response<boost::beast::http::string_body> CreditEndpoint::create_credit(const boost::beast::http::request<boost::beast::http::string_body>& req) {
    // Parse and validate JSON
    auto json = parse_rabbitmq_message(req.body());
    std::cout << "HUUUUUUUUU" << std::endl;

    // Required fields validation
    std::vector<std::string> required = {"user_id", "tariff_id", "amount"};
    for (const auto& field : required) {
        if (!json.template get_optional<std::string>(field)) {
            return app::errors::create_error_response(
                boost::beast::http::status::bad_request,
                "Missing required field: " + field,
                req
            );
        }
    }

    // UUID validation
    std::string user_id_str = json.template get<std::string>("user_id");
    std::string tariff_id_str = json.template get<std::string>("tariff_id");
    boost::optional<std::string> write_off_account_id_str =
        json.template get_optional<std::string>("write_off_account_id");

    if (!validate_uuid(user_id_str) || !validate_uuid(tariff_id_str)) {
        return app::errors::create_error_response(
            boost::beast::http::status::bad_request,
            "Invalid UUID format",
            req
        );
    }

    if (write_off_account_id_str && !validate_uuid(*write_off_account_id_str)) {
        return app::errors::create_error_response(
            boost::beast::http::status::bad_request,
            "Invalid write_off_account_id format",
            req
        );
    }

    // Amount validation
    double amount = json.template get<double>("amount");
    if (amount <= 0) {
        return app::errors::create_error_response(
            boost::beast::http::status::bad_request,
            "Amount must be positive",
            req
        );
    }
    // Send RabbitMQ message
    boost::property_tree::ptree message;
    message.put("user_id", user_id_str);
    message.put("tariff_id", tariff_id_str);
    message.put("amount", amount);
    if (write_off_account_id_str) {
        message.put("write_off_account_id", *write_off_account_id_str);
    }
    message.put("message", "Credit creation request");
    message.put("type_message", "creation_credit");

    std::ostringstream mq_oss;
    boost::property_tree::write_json(mq_oss, message);
    rabbitmq_.async_publish(mq_oss.str());
    int timeout_var = std::stoi(rabbitmq_.config_.at("CREATE_CREDIT_TIMEOUT_SEC"));
    std::chrono::seconds timeout_chrono = std::chrono::seconds(timeout_var);
    auto response = rabbitmq_.wait_for_response(
        "creation_credit",
        timeout_chrono,
        {{"user_id", user_id_str}}
    );

    boost::beast::http::response<boost::beast::http::string_body> res;
    res.set(boost::beast::http::field::content_type, "application/json");
    if (!response) {
        throw std::runtime_error("Failed to send credit creation request");
    }

    bool success = response->get<bool>("success", false);
    if (!success) {
        std::string error_msg = response->get<std::string>("error", "Credit creation failed");
        throw std::runtime_error(error_msg);
    }

    std::cout << user_id_str << ", " << tariff_id_str << ", " << amount << std::endl;

    const char* paramValues[4];
    paramValues[0] = user_id_str.c_str();
    paramValues[1] = tariff_id_str.c_str();
    std::string amount_str = std::to_string(amount);
    paramValues[2] = amount_str.c_str();
    paramValues[3] = write_off_account_id_str ?
        write_off_account_id_str->c_str() : nullptr;

    res.result(boost::beast::http::status::ok);
    std::ostringstream json_stream;
    boost::property_tree::write_json(json_stream, *response);
    res.body() = json_stream.str();

    res.prepare_payload();
    return res;
}

std::string CreditEndpoint::get_credit(std::string id) {
    std::promise<PGresult*> promise;
    auto future = promise.get_future();
    std::cout << "ID ID ID ID ID   " << id << std::endl;
    if (!id.empty()) {
        const char* params[] = {id.c_str()};
        db_.async_query_params(
            "SELECT * FROM credits WHERE id = ($1)",
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
        pt.add_child("credit", credits);

        std::ostringstream oss;
        boost::property_tree::write_json(oss, pt, false);
        std::string jsonStr = oss.str();

        std::regex amount_regex("\"amount\":\\s*\"([0-9\\.]+)\"");
        jsonStr = std::regex_replace(jsonStr, amount_regex, "\"amount\": $1");

        std::regex remaining_debt_regex("\"remaining_debt\":\\s*\"([0-9\\.]+)\"");
        jsonStr = std::regex_replace(jsonStr, remaining_debt_regex, "\"remaining_debt\": $1");

        return jsonStr;
    } else {
        const char* params[] = {};
        db_.async_query_params(
            "SELECT * FROM credits",
            params,
            0,
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
            credit.put("remaining_debt", PQgetvalue(res, i, 5));
            credits.push_back(std::make_pair("", credit));
        }

        PQclear(res);
        pt.add_child("credits", credits);

        std::ostringstream oss;
        boost::property_tree::write_json(oss, pt, false);
        std::string jsonStr = oss.str();

        std::regex remaining_debt_regex("\"remaining_debt\":\\s*\"([0-9\\.]+)\"");
        jsonStr = std::regex_replace(jsonStr, remaining_debt_regex, "\"remaining_debt\": $1");

        return jsonStr;
    }

}