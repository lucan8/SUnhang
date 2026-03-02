#include "../include/scc_enumerator.hpp"
#include "../include/logger.hpp"
#include <cassert>

MinSCC SCCEnumerator::get_min_strong_conn_comp(){
    for (const auto& [dep, _] : abs_deps_map){
        if (dep_info_map.find(&dep) == dep_info_map.end()) // Only look at unvisited nodes
            _get_min_strong_conn_comp(&dep);
    }

    return min_scc;
}

void SCCEnumerator::_get_min_strong_conn_comp(const AbsDependency* dep){
    
    // Visit node and check that it was not visited before
    auto dep_info_entry = dep_info_map.insert({dep, AbsDepInfo(max_index, max_index, true)});
    assert(dep_info_entry.second == true);
    

    // Alias only for the relevant part
    AbsDepInfo& dep_info = (dep_info_entry.first)->second;

    // Update max_index and stack
    max_index++;
    stack.push_back(dep);

    // DFS on neighbours, updating the dependeny low index if needed
    auto neigh_list_entry = neigh_list.find(dep);
    if (neigh_list_entry != neigh_list.end()){
        for (const auto& neigh : neigh_list_entry->second){
            auto neigh_info_entry = dep_info_map.find(neigh);
            if (neigh_info_entry == dep_info_map.end()){
                _get_min_strong_conn_comp(neigh);
                dep_info.low_index = std::min(dep_info.low_index, dep_info_map.at(neigh).low_index);
            }
            else{ // Trying to visit already visited node, if it's on the stack update the current node to be part of the scc of the older
                AbsDepInfo& neigh_info = neigh_info_entry->second;
                if (neigh_info.on_stack){
                    dep_info.low_index = std::min(dep_info.low_index, neigh_info.index);
                }
            }
        }
    }

    MinSCC scc;

    // Every node after this one(inclusive) will be part of the same new scc
    if (dep_info.low_index == dep_info.index){
        const AbsDependency* curr_dep;
        AbsDepInfo* curr_dep_info;

        do{
            // Remove from stack
            curr_dep = stack.back();
            curr_dep_info = &dep_info_map.find(curr_dep)->second;
            curr_dep_info->on_stack = false;
            stack.pop_back();
            
            // Add to the resulted scc
            scc.nodes.insert(curr_dep);
        }while (curr_dep_info->index != dep_info.index);

        // The minimum is always the one who gives the number of the SCC
        scc.min_node = curr_dep;

        // Update min_scc if the current scc has more than one node
        // It's minimum node will surely be smaller the current smallest because of the processing order
        if  (scc.nodes.size() > 1)
            min_scc = scc;
        
        // DEBUG: Track all SCCs created by tarjan's algorithm
        scc_vec.push_back(scc);
    }
}

void SCCEnumerator::print_info() const{
    Logger::print(LogType::INFO, "SCC INFORMATION");
    Logger::print_dash_line();

    Logger::print(LogType::DBG, "NUMBER OF SCCs: %d", scc_vec.size());
    for (const auto& scc : scc_vec){
        Logger::print(LogType::DBG, "NODES(%d):\n%s", scc.show().c_str(), scc.nodes.size());
    }

    Logger::print(LogType::INFO, "MIN SCC: %s", min_scc.show().c_str());
    Logger::print_dash_line();
}