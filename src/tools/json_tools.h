#include <string>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <ctime>
#include <sstream>

#ifndef JSON_TOOLS_H
#define JSON_TOOLS_H
std::string create_json_response(const std::string &key, const std::string &value);

#endif //JSON_TOOLS_H
