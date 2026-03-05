#include <cassert>
#include "../include/cycle_enumerator.hpp"
#include "../include/logger.hpp"


std::vector<NodeChainT> CycleEnumerator::enum_cycles(){
    int i = 0;
    while (!graph_view.empty()){
        SCCEnumerator scc_enum(graph_view);
        curr_min_scc = scc_enum.get_min_strong_conn_comp();

        // No SCC with more than one node, stop
        if (curr_min_scc.is_empty())
            break;
        
        Logger::print(LogType::DBG, "SCC {}: NODES({}):\n{}\n", i++, curr_min_scc.nodes.size(), curr_min_scc);
        
        graph_view.set_start_node(curr_min_scc.min_node);
        _reset_helper_structs(curr_min_scc);
        
        _enum_cycles(graph_view.start_node_it);
        graph_view.advance_start_node();
    }
    return res_cycles;
}

bool CycleEnumerator::_enum_cycles(NodeConstItT node){
    auto blocked_node = blocked_nodes.insert(node);
    assert(blocked_node.second == true);

    stack.push_back(node);
    bool cycle_on_curr_path = false;
    
    // Get the neigh list valid range
    auto neigh_list = graph_view.get_and_update_neigh_list_range(node);

    if (neigh_list.has_value()){
        for (const auto neigh : neigh_list.value()){
            // Cycle found
            if (neigh == graph_view.start_node_it){
                res_cycles.push_back(stack);
                cycle_on_curr_path = true;
                Logger::print(LogType::DBG, "CYCLE {}: NODES({}):\n{}\n", res_cycles.size(), res_cycles.back().size(), res_cycles.back());
            }
            // DFS on unvisited node, only looking at nodes in the current SCC!
            else if (curr_min_scc.has(neigh) && blocked_nodes.find(neigh) == blocked_nodes.end()){
                if (_enum_cycles(neigh)){
                    cycle_on_curr_path = true;
                }
            }
        }

        // Unblock node if cycle found, otherwise at it to the recursive block list of each neighbour
        if (cycle_on_curr_path){
            _unblock(node);
        }
        else{
            _rec_block(node, neigh_list.value());
        }

    }
    stack.pop_back();

    return cycle_on_curr_path;
}

void CycleEnumerator::_unblock(NodeConstItT node){
    // Unblock node
    blocked_nodes.erase(node);
    auto block_neigh_list = rec_block_map.find(node);

    if (block_neigh_list == rec_block_map.end())
        return;

    // Recursively unblock "neighbours"
    for (auto block_neigh : block_neigh_list->second){
        _unblock(block_neigh);
    }

    // Clear "neighbour" list
    block_neigh_list->second.clear();
}

// For each valid node in the neigh_list range add node to their set  
void CycleEnumerator::_rec_block(NodeConstItT node, NodeChainRangeT neigh_list){
     for (auto neigh : neigh_list){
        if (!curr_min_scc.has(neigh))
            continue;
        
        rec_block_map[neigh].insert(node);
    }
}

void CycleEnumerator::_reset_helper_structs(const MinSCC& curr_min_scc){
    // Eliminate the scc nodes from the blocked_nodes and from rec_block_map
    for (auto node : curr_min_scc.nodes){
        blocked_nodes.erase(node);
        
        auto rec_block_map_entry = rec_block_map.find(node);
        // Only clear the vector if the entry already exists
        if (rec_block_map_entry != rec_block_map.end()){
            rec_block_map_entry->second.clear();
        }
    }
}

void CycleEnumerator::print_info() const{
    Logger::print(LogType::INFO, "CYCLE INFORMATION");
    Logger::print_dash_line();

    Logger::print(LogType::INFO, "NUMBER OF CYCLES: {}", res_cycles.size());
    for (int i = 0; i < res_cycles.size(); ++i){
        Logger::print(LogType::DBG, "CYCLE {}: NODES({}):\n{}\n", i, res_cycles[i].size(), res_cycles[i]);
    }

    Logger::print_dash_line();
}