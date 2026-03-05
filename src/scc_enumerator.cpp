#include "../include/scc_enumerator.hpp"
#include "../include/logger.hpp"
#include <cassert>

MinSCC SCCEnumerator::get_min_strong_conn_comp(){
    for (auto node_it = graph_view.start_node_it; node_it != graph_view.get_nodes_end(); ++node_it){
        if (dep_info_map.find(node_it) == dep_info_map.end()) // Only look at unvisited nodes
            _get_min_strong_conn_comp(node_it);
    }

    return res_min_scc;
}

void SCCEnumerator::_get_min_strong_conn_comp(NodeConstItT dep){
    // Visit dep and check that it was not visited before
    auto dep_info_entry = dep_info_map.insert({dep, AbsDepInfo(max_index, max_index, true)});
    assert(dep_info_entry.second == true);
    

    // Alias only for the relevant part
    AbsDepInfo& dep_info = (dep_info_entry.first)->second;

    // Update max_index and stack
    max_index++;
    stack.push_back(dep);

    // Get the start neighbour of dep
    auto start_neigh_list_entry = graph_view.start_neigh_map.find(dep);
    if (start_neigh_list_entry != graph_view.start_neigh_map.end()){
        auto start_neigh = start_neigh_list_entry->second;

        // DFS on the valid neighbours
        for (auto neigh_it = start_neigh; neigh_it != graph_view.get_neigh_list_end(dep); ++neigh_it){
            NodeConstItT neigh = *neigh_it;
            auto neigh_info_entry = dep_info_map.find(neigh);

            // Recurse on unvisited dep
            if (neigh_info_entry == dep_info_map.end()){
                _get_min_strong_conn_comp(neigh);

                auto neigh_info_it =  dep_info_map.find(neigh);
                assert(neigh_info_it != dep_info_map.end()); // Remove in release mode
                
                dep_info.low_index = std::min(dep_info.low_index, neigh_info_it->second.low_index);
            }
            else{ // Update the low index if the neighbour is already visited and on the stack
                AbsDepInfo& neigh_info = neigh_info_entry->second;
                if (neigh_info.on_stack){
                    dep_info.low_index = std::min(dep_info.low_index, neigh_info.index);
                }
            }
        }
    }

    MinSCC scc(graph_view.get_nodes_end());

    // Every node after this one(inclusive) will be part of the same new scc
    if (dep_info.low_index == dep_info.index){
        NodeConstItT curr_dep;
        AbsDepInfo* curr_dep_info;

        do{
            // Remove from stack
            curr_dep = stack.back();
            curr_dep_info = &dep_info_map.find(curr_dep)->second;
            curr_dep_info->on_stack = false;
            stack.pop_back();
            
            // Add to the resulted scc
            scc.nodes.insert(curr_dep);
            
            // Update the min node if needed
            scc.min_node = std::min(scc.min_node, curr_dep, NodeItLess(scc.sentinel_node));
        }while (curr_dep_info->index != dep_info.index);

        // Update res_min_scc if the current scc has more than one node
        if  (scc.nodes.size() > 1){
            res_min_scc = std::min(res_min_scc, scc);
        }
        
        // DEBUG: Track all SCCs created by tarjan's algorithm
        res_scc_vec.push_back(scc);
    }
}

void SCCEnumerator::print_info() const{
    Logger::print(LogType::INFO, "SCC INFORMATION");
    Logger::print_dash_line();

    Logger::print(LogType::DBG, "NUMBER OF SCCs: {}", res_scc_vec.size());
    for (int i = 0; i < res_scc_vec.size(); ++i){
        Logger::print(LogType::DBG, "SCC {}: NODES({}):\n{}\n", i, res_scc_vec[i].nodes.size(), res_scc_vec[i]);
    }

    Logger::print(LogType::INFO, "MIN SCC: NODES({}):\n{}\n", res_min_scc.nodes.size(), res_min_scc);
    Logger::print_dash_line();
}