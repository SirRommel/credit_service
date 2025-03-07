#include "utils.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>




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