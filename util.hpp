#pragma once

#include <vector>
#include <string>
#include <sstream>
#include <cstdint>

const char trace_sep = '|';
const uint8_t exp_split_trace_size = 3;

// Splits str by sep
std::vector<std::string> split(const std::string& str, char sep){
    std::stringstream ss(str);
    std::vector<std::string> result;

    while(ss.good()) {
        std::string substring;
        getline(ss, substring, sep);
        result.push_back(substring);
    }

    return result;
}