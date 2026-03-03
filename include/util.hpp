#pragma once

#include <vector>
#include <string>
#include <sstream>
#include <cstdint>
#include "comm_types.hpp"

const char trace_sep = '|';
const uint8_t exp_split_trace_size = 3;

// Splits str by sep
inline std::vector<std::string> split(const std::string& str, char sep){
    std::stringstream ss(str);
    std::vector<std::string> result;

    while(ss.good()) {
        std::string substring;
        getline(ss, substring, sep);
        result.push_back(substring);
    }

    return result;
}

// Returns true if ls1 and ls2 intersect, false otherwise
// Currently iterates of ls2 and checks if all elements are in ls1
inline bool lockset_intersection(const LocksetT& ls1, const LocksetT& ls2){
    for (const auto lock : ls2)
        if (ls1.find(lock) != ls1.end())
            return true;
    return false;
}

// Generic struct used for pointer to object comparison
// Note: Nullptr is treated as infinity
struct PtrLess {
    template <typename T>
    bool operator()(const T* a, const T* b) const {
        if (a == b) 
            return false;
        
        // a=nullptr, b=val -> false, a=val, b=nullptr->true, made like this to help min!
        if (!a || !b) 
            return a > b;
        
        return *a < *b;
    }
};