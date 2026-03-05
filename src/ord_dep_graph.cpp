#include "../include/ord_dep_graph.hpp"

void OrdDepGraphView::init_start_structs(){
    set_start_node();
    set_start_neigh_map();
}

void OrdDepGraphView::set_start_node(){
    start_node_it = graph.abs_deps_map.begin();
}

void OrdDepGraphView::set_start_node(NodeConstItT node){
    start_node_it = node;
}

void OrdDepGraphView::advance_start_node(){
    start_node_it = std::next(start_node_it);
}

void OrdDepGraphView::set_start_neigh_map(){
    for (const auto& [dep, neigh_list] : graph.neigh_list)
        start_neigh_map.emplace(dep, neigh_list.begin());
}

std::optional<NodeChainRangeT> OrdDepGraphView::get_and_update_neigh_list_range(NodeConstItT node) {
    // Define the current valid range
    std::optional<NodeChainConstItT> end_opt = get_neigh_list_end(node);
    if (!end_opt.has_value())
        return {};
    
    auto curr_start_entry = start_neigh_map.find(node);

    // Binary search for the new start and update
    NodeChainConstItT new_start = std::lower_bound(curr_start_entry->second, end_opt.value(), start_node_it, NodeItLess(get_nodes_end()));
    curr_start_entry->second = new_start;

    return {NodeChainRangeT(new_start, end_opt.value())};
}


NodeConstItT OrdDepGraphView::get_real_nodes_start() const{
    return graph.abs_deps_map.begin();
}

NodeConstItT OrdDepGraphView::get_sentinel_node() const{
    return graph.abs_deps_map.end();
}

NodeConstItT OrdDepGraphView::get_nodes_end() const{
    return graph.abs_deps_map.end();
}

std::optional<NodeChainConstItT> OrdDepGraphView::get_neigh_list_end(NodeConstItT dep) const{
    auto neigh_list_it = graph.neigh_list.find(dep);
    if (neigh_list_it == graph.neigh_list.end())
        return {};

    return neigh_list_it->second.end();
}

bool OrdDepGraphView::empty() const{
    return start_node_it == get_nodes_end();
}