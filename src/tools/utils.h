#include <string>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <ctime>
#include <map>
#include <sstream>
#include <boost/asio.hpp>

#ifndef UTILS_H
#define UTILS_H
void send_response(boost::asio::ip::tcp::socket &socket, const std::string &status, const std::string &json_response);
std::map<std::string, std::string> read_env_file(const std::string& filename);
#endif //UTILS_H
