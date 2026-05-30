#include "../include/scc_enumerator.hpp"
#include "../include/logger.hpp"


SCCEnumerator::SCCEnumerator(OrdDepGraphView& graph_view)
    : graph_view(graph_view), max_index(0), res_min_scc(graph_view.get_nodes_end()){}

MinSCC SCCEnumerator::get_min_strong_conn_comp(){
    for (auto node_it = graph_view.start_node_it; node_it != graph_view.get_nodes_end(); ++node_it){
        if (node_info_map.find(node_it) == node_info_map.end()) // Only look at unvisited nodes
            _get_min_strong_conn_comp(node_it);
    }

    return res_min_scc;
}

void SCCEnumerator::_get_min_strong_conn_comp(NodeConstItT node){
    // Visit node and check that it was not visited before
    auto node_info_entry = node_info_map.insert({node, NodeInfo(max_index, max_index, true)});
    // assert(node_info_entry.second == true);

    // Alias only for the relevant part
    NodeInfo& node_info = (node_info_entry.first)->second;

    // Update max_index and stack
    max_index++;
    stack.push_back(node);

    // Get the valid neighbour list of nodes
    // This is a must especially if running together with the cycle enumerator which "kills" nodes
    // by moving the start pointer of the graph
    auto neigh_list = graph_view.get_and_update_neigh_list_range(node);

    if (neigh_list.has_value()){
        // DFS on the valid neighbours
        for (auto neigh : neigh_list.value()){
            auto neigh_info_entry = node_info_map.find(neigh);

            // Recurse on unvisited node
            if (neigh_info_entry == node_info_map.end()){
                _get_min_strong_conn_comp(neigh);

                auto neigh_info_it =  node_info_map.find(neigh);
                // assert(neigh_info_it != node_info_map.end()); // Remove in release mode
                
                // There might be a path from our neighbour to an earlier node
                // This means we also have a path to that node(low_index is the id of that smaller node)
                node_info.low_index = std::min(node_info.low_index, neigh_info_it->second.low_index);
            }
            else{ // Update the low index if the neighbour is already visited and on the stack
                NodeInfo& neigh_info = neigh_info_entry->second;
                // If visited and on the stack it is part of the same dfs sequence
                // Which means there is a path from this neighbour to us
                // If before us, we are part of the same scc
                if (neigh_info.on_stack){
                    node_info.low_index = std::min(node_info.low_index, neigh_info.index);
                }
            }
        }
    }
    MinSCC scc(graph_view.get_nodes_end());

    // Every node after this one(inclusive) will be part of the same new scc
    if (node_info.low_index == node_info.index){
        NodeConstItT curr_node;
        NodeInfo* curr_node_info;

        do{
            // Remove from stack
            curr_node = stack.back();
            curr_node_info = &node_info_map.find(curr_node)->second;
            curr_node_info->on_stack = false;
            stack.pop_back();
            
            // Add to the resulted scc
            scc.nodes.insert(curr_node);
            
            // Update the min node if needed
            scc.min_node = std::min(scc.min_node, curr_node, NodeItLess(scc.sentinel_node));
        }while (curr_node_info->index != node_info.index);

        // Update res_min_scc if the current scc has more than one node
        if  (scc.nodes.size() > 1){
            res_min_scc = std::min(res_min_scc, scc);
        }
        
        // DEBUG: Track all SCCs created by tarjan's algorithm
        // res_scc_vec.push_back(scc);
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