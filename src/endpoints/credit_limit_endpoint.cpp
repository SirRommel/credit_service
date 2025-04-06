// CreditLimitEndpoint.cpp
#include "credit_limit_endpoint.h"

#include <iostream>

#include "../tools/json_tools.h"
#include <boost/algorithm/string.hpp>
#include <regex>
#include "../tools/utils.h"

#include "errors/http_errors.h"



boost::beast::http::response<boost::beast::http::string_body> CreditLimitEndpoint::handle(const boost::beast::http::request<boost::beast::http::string_body>& req) {
    try {
        std::string user_id = extract_id_from_path(std::string(req.target()));
        if (user_id.empty()) {
            return app::errors::create_error_response(
                boost::beast::http::status::bad_request,
                "Invalid UUID format",
                req
            );
        }

        std::promise<PGresult*> promise;
        const char* paramValues[] = {user_id.c_str()};
        db_.async_query_params(
            "SELECT rating FROM credit_history WHERE user_id = $1",
            paramValues,
            1,
            [&](PGresult* res) { promise.set_value(res); }
        );
        PGresult* select_res = promise.get_future().get();

        double rating = 50.0; // Значение по умолчанию
        bool exists = PQntuples(select_res) > 0;

        std::cout << "HEEEEEEEEEy" << std::endl;

        if (!exists) {
            std::cout << "FFFFFFFFFOWIOFIWO" << std::endl;
            std::promise<PGresult*> insert_promise;
            const char* insertParams[] = {user_id.c_str(), "50"};
            db_.async_query_params(
                "INSERT INTO credit_history (user_id, rating) VALUES ($1, $2)",
                insertParams,
                2,
                [&](PGresult* res) { insert_promise.set_value(res); }
            );

            PGresult* insert_res = insert_promise.get_future().get();
            PQclear(insert_res);
        } else {
            rating = std::stod(PQgetvalue(select_res, 0, 0));
        }
        PQclear(select_res);


        double limit = rating * 1000;
        boost::property_tree::ptree pt;
        pt.put("limit", limit);

        boost::beast::http::response<boost::beast::http::string_body> res(boost::beast::http::status::ok, 11);
        res.set(boost::beast::http::field::content_type, "application/json");

        std::ostringstream oss;
        boost::property_tree::write_json(oss, pt);
        std::string jsonStr = oss.str();

        std::regex limit_regex("\"limit\":\\s*\"([0-9\\.]+)\"");
        jsonStr = std::regex_replace(jsonStr, limit_regex, "\"limit\": $1");

        res.body() = jsonStr;
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