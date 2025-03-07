// CreditLimitEndpoint.cpp
#include "credit_limit_endpoint.h"

#include <iostream>

#include "../tools/json_tools.h"
#include <boost/algorithm/string.hpp>
#include <regex>
#include "../tools/json_tools.h"

#include "errors/http_errors.h"

std::string CreditLimitEndpoint::extract_user_id(const std::string& path) {
    std::regex uuid_pattern(
        "^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$"
    );
    std::smatch matches;
    std::string user_id = path.substr(path.rfind('/') + 1);

    if (std::regex_match(user_id, matches, uuid_pattern)) {
        return user_id;
    }
    return "";
}

boost::beast::http::response<boost::beast::http::string_body> CreditLimitEndpoint::handle(const boost::beast::http::request<boost::beast::http::string_body>& req) {
    try {
        std::string user_id = extract_user_id(req.target());
        if (user_id.empty()) {
            return app::errors::create_error_response(
                boost::beast::http::status::bad_request,
                "Invalid UUID format",
                req
            );
        }

        // Проверка существования записи и получение рейтинга
        std::promise<PGresult*> promise;
        std::string select_query = "SELECT rating FROM credit_history WHERE user_id = '" + user_id + "'";
        db_.async_query(select_query, [&](PGresult* res) { promise.set_value(res); });
        PGresult* select_res = promise.get_future().get();

        double rating = 50.0; // Значение по умолчанию
        bool exists = PQntuples(select_res) > 0;

        if (!exists) {
            std::cout << "FFFFFFFFFOWIOFIWO" << std::endl;
            // Создаем новую запись с рейтингом 50
            std::string insert_query = "INSERT INTO credit_history (user_id, rating) VALUES ('" + user_id + "', 50)";
            std::promise<PGresult*> insert_promise;
            db_.async_query(insert_query, [&](PGresult* res) { insert_promise.set_value(res); });
            PGresult* insert_res = insert_promise.get_future().get();
            PQclear(insert_res);
        } else {
            // Получаем существующий рейтинг
            rating = std::stod(PQgetvalue(select_res, 0, 0));
        }
        PQclear(select_res);

        // Формируем ответ
        double limit = rating * 1000;
        boost::property_tree::ptree pt;
        pt.put("limit", limit);

        boost::beast::http::response<boost::beast::http::string_body> res(boost::beast::http::status::ok, 11);
        res.set(boost::beast::http::field::content_type, "application/json");

        std::ostringstream oss;
        boost::property_tree::write_json(oss, pt);
        res.body() = oss.str();
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