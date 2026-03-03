#pragma once

#include "predictor_types.hpp"

struct CycleEnumerator{
    // Holds the resulted cycles from enum_cycles
    std::vector<std::vector<const AbsDependency*>> res_cycles;
    
    // Helper structures for the actual cycle algorithm
    std::unordered_set<const AbsDependency*> blocked_nodes;
    std::unordered_map<const AbsDependency*, const AbsDependency*> rec_block_map;
    
    std::vector<std::vector<const AbsDependency*>> enum_cycles();

    void _enum_cycles(const AbsDependency* dep);
};
