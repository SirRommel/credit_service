//
// Created by rommel on 3/5/25.
//
#include <map>
#include <string>
#include <fstream>
#include <iostream>
#include <sstream>

#ifndef ENV_CONFIG_H
#define ENV_CONFIG_H
std::map<std::string, std::string> read_env_file(const std::string& filename);
#endif //ENV_CONFIG_H
