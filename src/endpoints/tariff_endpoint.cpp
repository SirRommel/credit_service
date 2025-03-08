#include "tariff_endpoint.h"

#include <iostream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/algorithm/string.hpp>
#include <sstream>
#include <regex>

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
                std::string id = extract_id_from_path(req.target());
                std::string result = get_all_tariffs(id);
                boost::beast::http::response<boost::beast::http::string_body> res{boost::beast::http::status::ok, req.version()};
                res.set(boost::beast::http::field::content_type, "application/json");
                res.body() = result;
                return res;
            }
            else {
                std::string id = extract_id_from_path(req.target());
                if (id.empty()) {
                    return errors::create_error_response(
                        http::status::bad_request,
                        "Invalid UUID format",
                        req
                    );
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

            std::string employee_id = pt.get<std::string>("employee_id");
            std::string name = pt.get<std::string>("name");
            double interest_rate = pt.get<double>("interest_rate");

            // Валидация
            if (interest_rate < 0 || interest_rate > 100) {
                throw std::invalid_argument("Interest rate must be between 0 and 100");
            }

            // Вставка в БД
            std::promise<PGresult*> promise;
            auto future = promise.get_future();
            std::string interest_rate_str = std::to_string(interest_rate);
            const char* params[] = {employee_id.c_str(), name.c_str(), interest_rate_str.c_str()};
            db_.async_query_params(
                "INSERT INTO tariffs (employee_id, name, interest_rate) VALUES ($1, $2, $3) RETURNING id",
                params,
                3,
                [&](PGresult* res) { promise.set_value(res); }
            );

            PGresult* res = future.get();
            if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                PQclear(res);
                throw std::runtime_error("Failed to create tariff");
            }

            std::string new_id = PQgetvalue(res, 0, 0);
            PQclear(res);

            return "Tariff created with ID: " + new_id;
        }

        std::string TariffEndpoint::get_all_tariffs(std::string id) {
            std::promise<PGresult*> promise;
            auto future = promise.get_future();
            std::cout << "ID ID ID ID ID   " << id << std::endl;
            if (!id.empty()) {
                const char* params[] = {id.c_str()};
                db_.async_query_params(
                    "SELECT * FROM tariffs WHERE id = ($1)",
                    params,
                    1,
                    [&](PGresult* res) { promise.set_value(res); }
                );

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
            } else {
                const char* params[] = {};
                db_.async_query_params(
                    "SELECT * FROM tariffs",
                    params,
                    0,
                    [&](PGresult* res) { promise.set_value(res); }
                );

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

        }

            std::string TariffEndpoint::update_tariff(std::string id, const std::string & body) {
                std::stringstream ss(body);
                boost::property_tree::ptree pt;
                boost::property_tree::read_json(ss, pt);
        

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
                std::vector<std::string> paramValues;
                for (const auto& update : updates) {
                    size_t pos = update.find("=");
                    if (pos != std::string::npos) {
                        paramValues.push_back(update.substr(pos + 2)); // Извлекаем значение после " = "
                    }
                }
                paramValues.push_back(id);

                std::promise<PGresult*> promise;
                auto future = promise.get_future();
                std::ostringstream query;
                query << "UPDATE tariffs SET ";
                for (size_t i = 0; i < updates.size(); ++i) {
                    query << updates[i].substr(0, updates[i].find("=")) << " = $" << (i + 1);
                    if (i < updates.size() - 1) query << ", ";
                }
                query << " WHERE id = $" << (paramValues.size());

                // Вызываем async_query_params
                const char* params[paramValues.size()];
                std::transform(paramValues.begin(), paramValues.end(), params, [](const std::string& s) { return s.c_str(); });

                db_.async_query_params(query.str(), params, paramValues.size(), [&](PGresult* res) { promise.set_value(res); });

                PGresult* res = future.get();
                if (PQresultStatus(res) != PGRES_COMMAND_OK) {
                    PQclear(res);
                    throw std::runtime_error("Update failed: " + db_.get_last_error());
                }

                PQclear(res);
                return "Tariff updated successfully";
            }

        std::string TariffEndpoint::delete_tariff(std::string id) {
            std::promise<PGresult*> promise;
            auto future = promise.get_future();

            const char* params[] = { id.c_str() };
            db_.async_query_params("DELETE FROM tariffs WHERE id = $1", params, 1, [&](PGresult* res) { promise.set_value(res); });

            PGresult* res = future.get();
            if (PQresultStatus(res) != PGRES_COMMAND_OK) {
                PQclear(res);
                throw std::runtime_error("Delete failed");
            }

            PQclear(res);
            return "Tariff deleted successfully";
        }

        std::string TariffEndpoint::extract_id_from_path(const std::string& path) {
            std::string uuid_regex_str = "[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}";
            std::regex uuid_pattern(uuid_regex_str);
            std::smatch matches;

            if (std::regex_search(path.begin(), path.end(), matches, uuid_pattern)) {
                return matches.str();
            }
            return "";
        }

        } // namespace endpoints
} // namespace app