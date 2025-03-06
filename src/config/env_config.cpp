#include "env_config.h"


std::map<std::string, std::string> read_env_file(const std::string& filename) {
    std::map<std::string, std::string> env_map;
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open .env file." << std::endl;
        return env_map;
    }

    std::string line;
    while (std::getline(file, line)) {
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            env_map[key] = value;
        }
    }
    file.close();
    return env_map;
}