#ifndef BASE_DB_MODEL_H
#define BASE_DB_MODEL_H

#include <iostream>
#include <sstream>
#include <vector>
#include <memory>

class BaseModel {
public:
    virtual ~BaseModel() = default;
    virtual std::string get_table_name() const = 0;
    virtual std::vector<std::string> get_columns() const = 0;

    std::string get_create_table_query() const {
        std::ostringstream query;
        query << "CREATE TABLE IF NOT EXISTS " << get_table_name() << " (";

        auto columns = get_columns();
        for (size_t i = 0; i < columns.size(); ++i) {
            query << columns[i];
            if (i < columns.size() - 1) {
                query << ", ";
            }
        }

        query << ");";
        return query.str();
    }
};

#endif //BASE_DB_MODEL_H
