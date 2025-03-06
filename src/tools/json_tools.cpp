#include "json_tools.h"

//std::string create_json_response(const std::string &key, const std::string &value) {
//    boost::property_tree::ptree root;
//    root.put(key, value);
//
//    std::ostringstream oss; // Теперь этот класс доступен
//    boost::property_tree::write_json(oss, root);
//    return oss.str();
//}

std::string create_json_response(int id, const std::string& message) {
    boost::property_tree::ptree root;
    root.put("id", id);
    root.put("message", message);
    std::ostringstream oss;
    boost::property_tree::write_json(oss, root);
    return oss.str();
}