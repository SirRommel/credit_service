#include "tariff_endpoint.h"

#include <iostream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/algorithm/string.hpp>
#include <sstream>
#include "../db/database_manager.h"
#include "errors/http_errors.h"

namespace app {
namespace endpoints {
    boost::beast::http::response<boost::beast::http::string_body>
    TariffEndpoint::handle(const boost::beast::http::request<boost::beast::http::string_body>& req) {
        try {
            if (req.method() == boost::beast::http::verb::post) {
                std::string result = create_tariff(req.body());
                boost::beast::http::response<boost::beast::http::string_body> res{boost::beast::http::status::created, req.version()};
                res.set(boost::beast::http::field::content_type, "application/json");
                res.body() = result;
                return res;
            }
            else if (req.method() == boost::beast::http::verb::get) {
                std::string result = get_all_tariffs();
                boost::beast::http::response<boost::beast::http::string_body> res{boost::beast::http::status::ok, req.version()};
                res.set(boost::beast::http::field::content_type, "application/json");
                res.body() = result;
                return res;
            }
            else {
                int id = extract_id_from_path(req.target());
                if (id <= 0) {
                    return errors::not_found(req);
                }

                if (req.method() == boost::beast::http::verb::put) {
                    std::string result = update_tariff(id, req.body());
                    boost::beast::http::response<boost::beast::http::string_body> res{boost::beast::http::status::ok, req.version()};
                    res.set(boost::beast::http::field::content_type, "application/json");
                    res.body() = result;
                    return res;
                }

                if (req.method() == boost::beast::http::verb::delete_) {
                    std::string result = delete_tariff(id);
                    boost::beast::http::response<boost::beast::http::string_body> res{boost::beast::http::status::ok, req.version()};
                    res.set(boost::beast::http::field::content_type, "application/json");
                    res.body() = result;
                    return res;
                }
                else {
                    return errors::method_not_allowed(req);
                }
            }


        } catch (const std::invalid_argument& e) {
            return errors::create_error_response(
                boost::beast::http::status::bad_request,
                e.what(),
                req);
        } catch (const std::exception& e) {
            return errors::create_error_response(
                boost::beast::http::status::internal_server_error,
                e.what(),
                req);
        }

        return errors::method_not_allowed(req);
    }

std::string TariffEndpoint::create_tariff(const std::string& body) {
    // Парсинг JSON
    std::stringstream ss(body);
    boost::property_tree::ptree pt;
    boost::property_tree::read_json(ss, pt);

    int employee_id = pt.get<int>("employee_id");
    std::string name = pt.get<std::string>("name");
    double interest_rate = pt.get<double>("interest_rate");

    // Валидация
    if (interest_rate < 0 || interest_rate > 100) {
        throw std::invalid_argument("Interest rate must be between 0 and 100");
    }

    // Вставка в БД
    std::promise<PGresult*> promise;
    auto future = promise.get_future();

    std::string query = "INSERT INTO tariffs (employee_id, name, interest_rate) "
                        "VALUES (" + std::to_string(employee_id) + ", '" +
                        name + "', " + std::to_string(interest_rate) + ") RETURNING id";

    db_.async_query(query, [&](PGresult* res) {
        promise.set_value(res);
    });

    PGresult* res = future.get();
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        throw std::runtime_error("Failed to create tariff");
    }

    int new_id = std::stoi(PQgetvalue(res, 0, 0));
    PQclear(res);

    return "Tariff created with ID: " + std::to_string(new_id);
}

std::string TariffEndpoint::get_all_tariffs() {
    std::promise<PGresult*> promise;
    auto future = promise.get_future();

    db_.async_query("SELECT * FROM tariffs", [&](PGresult* res) {
        promise.set_value(res);
    });

    PGresult* res = future.get();
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        throw std::runtime_error("Failed to fetch tariffs");
    }

    // Формирование JSON
    boost::property_tree::ptree pt;
    boost::property_tree::ptree tariffs;

    int rows = PQntuples(res);
    for (int i = 0; i < rows; ++i) {
        boost::property_tree::ptree tariff;
        tariff.put("id", PQgetvalue(res, i, 0));
        tariff.put("employee_id", PQgetvalue(res, i, 1));
        tariff.put("name", PQgetvalue(res, i, 2));
        tariff.put("interest_rate", PQgetvalue(res, i, 3));
        tariffs.push_back(std::make_pair("", tariff));
    }

    PQclear(res);
    pt.add_child("tariffs", tariffs);

    std::ostringstream oss;
    boost::property_tree::write_json(oss, pt);
    return oss.str();
}

    std::string TariffEndpoint::update_tariff(int id, const std::string & body) {
        std::stringstream ss(body);
        boost::property_tree::ptree pt;
        boost::property_tree::read_json(ss, pt);

        std::ostringstream query;
        query << "UPDATE tariffs SET ";

        std::vector<std::string> updates;

        if (pt.count("employee_id")) {
            updates.push_back("employee_id = " + std::to_string(pt.get<int>("employee_id")));
        }
        if (pt.count("name")) {
            updates.push_back("name = '" + pt.get<std::string>("name") + "'");
        }
        if (pt.count("interest_rate")) {
            double rate = pt.get<double>("interest_rate");
            if (rate < 0 || rate > 100) {
                throw std::invalid_argument("Interest rate must be between 0 and 100");
            }
            updates.push_back("interest_rate = " + std::to_string(rate));
        }

        if (updates.empty()) {
            throw std::invalid_argument("No fields to update");
        }

        query << boost::algorithm::join(updates, ", "); // Правильное формирование запроса
        query << " WHERE id = " << id;

        std::promise<PGresult*> promise;
        auto future = promise.get_future();

        db_.async_query(query.str(), [&](PGresult* res) {
            promise.set_value(res);
        });

        PGresult* res = future.get();
        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            PQclear(res);
            throw std::runtime_error("Update failed: " + db_.get_last_error());
        }

        PQclear(res);
        return "Tariff updated successfully";
    }

std::string TariffEndpoint::delete_tariff(int id) {
    std::promise<PGresult*> promise;
    auto future = promise.get_future();

    std::string query = "DELETE FROM tariffs WHERE id = " + std::to_string(id);
    db_.async_query(query, [&](PGresult* res) {
        promise.set_value(res);
    });

    PGresult* res = future.get();
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        PQclear(res);
        throw std::runtime_error("Delete failed");
    }

    PQclear(res);
    return "Tariff deleted successfully";
}

int TariffEndpoint::extract_id_from_path(const std::string& path) {
    size_t pos = path.find_last_of('/');
    if (pos == std::string::npos) return -1;
    std::string id_str = path.substr(pos + 1);
    return std::stoi(id_str);
}

} // namespace endpoints
} // namespace app