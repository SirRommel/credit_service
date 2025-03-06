#include "json_tools.h"

std::string create_json_response(const std::string &key, const std::string &value) {
    boost::property_tree::ptree root;
    root.put(key, value);

    std::ostringstream oss; // Теперь этот класс доступен
    boost::property_tree::write_json(oss, root);
    return oss.str();
}