#include <string>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <ctime>
#include <map>
#include <sstream>
#include <boost/asio.hpp>

#ifndef UTILS_H
#define UTILS_H
std::map<std::string, std::string> read_env_file(const std::string& filename);
std::string extract_id_from_path(const std::string& path);
#endif //UTILS_H
