#include "utils.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>


void send_response(boost::asio::ip::tcp::socket &socket, const std::string &status, const std::string &json_response) {
    std::ostringstream response_stream;
    response_stream
        << "HTTP/1.1 " << status << "\r\n"
        << "Content-Type: application/json\r\n"
        << "Content-Length: " << json_response.length() << "\r\n"
        << "\n"
        << json_response;

    boost::asio::write(socket, boost::asio::buffer(response_stream.str()));
}

std::map<std::string, std::string> read_env_file(const std::string& filename) {
    std::map<std::string, std::string> env_map;
    std::ifstream file(filename);

    if (!file.is_open()) {
        std::cerr << "Error: Could not open " << filename << " file." << std::endl;
        return env_map;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Пропускаем пустые строки и комментарии
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        std::string key, value;

        if (std::getline(iss, key, '=') && std::getline(iss, value)) {
            env_map[key] = value;
        }
    }
    return env_map;
}