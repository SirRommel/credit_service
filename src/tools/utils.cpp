#include "utils.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <regex>


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

std::string extract_id_from_path(boost::beast::string_view path) {
    std::string uuid_regex_str = "[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}";
    std::regex uuid_pattern(uuid_regex_str);
    std::smatch matches;

    // Преобразуем string_view в std::string для совместимости с regex
    std::string path_str(path.begin(), path.end());

    if (std::regex_search(path_str, matches, uuid_pattern)) {
        return matches.str();
    }
    return "";
}