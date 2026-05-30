#include "../include/ord_dep_graph.hpp"
#include "../include/logger.hpp"

OrdDepGraph::OrdDepGraph(AbsDepContainerT&& abs_deps, const LockDepMapT& lock_dep_map) 
    : nodes(std::move(abs_deps)){
    _build_neigh_list(lock_dep_map);
}

void OrdDepGraph::_build_neigh_list(const LockDepMapT& lock_dep_map) {
        for (auto node_it = nodes.begin(); node_it != nodes.end(); ++node_it){
        // Get candidate neighbours
        auto lock_dep_it = lock_dep_map.find(node_it->resource_id);
        if (lock_dep_it == lock_dep_map.end())
            continue;
        
        // Add valid candidates to the neigbour list of dep
        for (auto cand : lock_dep_it->second._vec)
            if (node_it->is_valid_neigh_cand_soft(*cand))
                neigh_list[node_it].push_back(cand);
    }

    // Sort the neighbour list of each node, this will be needed later
    auto sentinel_node = NodeItLess(nodes.end());
    for (auto& [dep, _neigh_list] : neigh_list){
        std::sort(_neigh_list.begin(), _neigh_list.end(), sentinel_node);
    }
}

void OrdDepGraph::print_neigh_list(std::FILE* out_file) const{
    Logger::print(out_file, "NEIGHBOUR LIST");
    Logger::print(out_file, "------------------------------------");

    for (const auto& [dep, neigh_list] : neigh_list){
        Logger::print(out_file, "{}(dep): {}(neigh count)", *dep, neigh_list.size());
        for (const auto neigh : neigh_list)
            Logger::print(out_file, "{}", *neigh);
    }

    Logger::print(out_file, "Num deps that have neigh: {}", neigh_list.size());
    Logger::print(out_file, "------------------------------------");
}

size_t OrdDepGraph::get_dep_count() const{
    return nodes.size();
}

size_t OrdDepGraph::get_lock_dep_count() const{
    size_t lock_dep_count = 0;

    for (const auto& dep: nodes){
        if (dep.is_lock_dep()){
            lock_dep_count += 1;
        }
    }
    
    return lock_dep_count;
}

std::pair<size_t, size_t> OrdDepGraph::get_split_dep_counts() const{
    size_t lock_dep_count = get_lock_dep_count();
    size_t cond_dep_count = get_dep_count() - lock_dep_count;
    return {lock_dep_count, cond_dep_count};
}

void OrdDepGraphView::init_start_structs(){
    set_start_node();
    set_start_neigh_map();
}

void OrdDepGraphView::set_start_node(){
    start_node_it = graph.nodes.begin();
}

void OrdDepGraphView::set_start_node(NodeConstItT node){
    start_node_it = node;
}

void OrdDepGraphView::advance_start_node(){
    start_node_it = std::next(start_node_it);
}

void OrdDepGraphView::set_start_neigh_map(){
    for (const auto& [dep, neigh_list] : graph.neigh_list){
        start_neigh_map.emplace(dep, neigh_list.begin());
    }
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
    return graph.nodes.begin();
}

NodeConstItT OrdDepGraphView::get_sentinel_node() const{
    return graph.nodes.end();
}

NodeConstItT OrdDepGraphView::get_nodes_end() const{
    return graph.nodes.end();
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