// CreditEndpoint.cpp
#include "credit_endpoint.h"
#include "../tools/json_tools.h"
#include "../errors/http_errors.h"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/random_generator.hpp>
#include <regex>
#include <chrono>

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
        // Parse and validate JSON
        auto json = parse_rabbitmq_message(req.body());

        // Required fields validation
        std::vector<std::string> required = {"user_id", "tariff_id", "amount"};
        for (const auto& field : required) {
            if (!json.get_optional<std::string>(field)) {
                return app::errors::create_error_response(
                    boost::beast::http::status::bad_request,
                    "Missing required field: " + field,
                    req
                );
            }
        }

        // UUID validation
        std::string user_id_str = json.get<std::string>("user_id");
        std::string tariff_id_str = json.get<std::string>("tariff_id");
        boost::optional<std::string> write_off_account_id_str =
            json.get_optional<std::string>("write_off_account_id");

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
        double amount = json.get<double>("amount");
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
        message.put("amount", amount);
        if (write_off_account_id_str) {
            message.put("write_off_account_id", *write_off_account_id_str);
        }
        message.put("message", "Credit creation request");
        message.put("type_message", "creation_credit");

        std::ostringstream mq_oss;
        boost::property_tree::write_json(mq_oss, message);
        rabbitmq_.async_publish(mq_oss.str());

        // Wait for response
        auto response = rabbitmq_.wait_for_response(
            "creation_credit",
            60s,
            {{"user_id", user_id_str}}
        );

        // Prepare HTTP response
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

        std::promise<PGresult*> db_promise;
        db_.async_query_params(
            "INSERT INTO credits (user_id, tariff_id, amount, write_off_account_id) "
            "VALUES ($1, $2, $3, $4)",
            paramValues,
            4,
            [&](PGresult* res) { db_promise.set_value(res); } // Используем объявленный db_promise
        );

        PGresult* db_result = db_promise.get_future().get();
        if (PQresultStatus(db_result) != PGRES_COMMAND_OK) {
            std::string error = PQerrorMessage(db_.get_connection());
            PQclear(db_result);
            throw std::runtime_error("Database error: " + error);
        }
        PQclear(db_result);

        res.result(boost::beast::http::status::ok);
        std::ostringstream json_stream;
        boost::property_tree::write_json(json_stream, *response);
        res.body() = json_stream.str();

        res.prepare_payload();
        return res;

    } catch (const std::exception& e) {
        return app::errors::create_error_response(
            boost::beast::http::status::internal_server_error,
            e.what(),
            req
        );
    }
}