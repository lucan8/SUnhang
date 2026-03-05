#pragma once

#include <map>
#include <cassert>
#include <ranges>
#include <format>
#include <optional>

#include "predictor_types.hpp"

typedef std::map<AbsDependency, std::vector<VectorClock>> NodeContainerT;
typedef std::map<AbsDependency, std::vector<VectorClock>>::const_iterator NodeConstItT;

typedef std::vector<NodeConstItT> NodeChainT;
typedef std::vector<NodeConstItT>::const_iterator NodeChainConstItT;
typedef std::ranges::subrange<NodeChainConstItT> NodeChainRangeT;

typedef std::unordered_set<NodeConstItT, IteratorHasher, IteratorHasher> NodeSetT;

typedef std::unordered_map<NodeConstItT, NodeChainT, IteratorHasher, IteratorHasher> NeighListT;

// Format for LocksetT
template <>
struct std::formatter<NodeChainT> : std::formatter<std::string> {
    auto format(const NodeChainT& node_chain, format_context& ctx) const {
        auto out = ctx.out();
        for (const auto& node : node_chain)
          std::format_to(out, "{}, ", node->first);
        return out;
    }
};

// less on NodeConstItT
// sentinel_node should be a valid sentilen for both nodes otherwise you will undefined behaviour
struct NodeItLess{
    NodeConstItT sentinel_node;
    NodeItLess(NodeConstItT sentinel_node): sentinel_node(sentinel_node){}
    
    bool operator()(NodeConstItT node1, NodeConstItT node2) const{
        if (node1 == node2) 
            return false;
        
        if (!is_valid_iter(node1, sentinel_node))
            return false;

        if (!is_valid_iter(node2, sentinel_node))
            return true;
        
        return node1->first < node2->first;
    }
};

// Struct that just holds together nodes and edges
struct OrdDepGraph{
    // AbsDependency represents a node the graph
    NodeContainerT abs_deps_map;

    // The vector in neigh list is ordered
    NeighListT neigh_list;
};

// Struct that exposes a view on a graph by keeping pointers to the first valid node of the graph
// And to the first valid neighbour of each node
struct OrdDepGraphView{
    OrdDepGraph graph;

    // Reference to the current valid node in the graph
    NodeConstItT start_node_it;

    // Map that gives the start neighbour of each node
    std::unordered_map<NodeConstItT, NodeChainConstItT, IteratorHasher> start_neigh_map;

    // Initializes start_node_it and start_neigh_map
    void init_start_structs(){
        set_start_node();
        set_start_neigh_map();
    }

    // Initializes start_node_it to point to the first node in graph.abs_dep_map
    void set_start_node(){
        start_node_it = graph.abs_deps_map.begin();
    }

    // Initializes start_node_it to point to node
    void set_start_node(NodeConstItT node){
        start_node_it = node;
    }

    void advance_start_node(){
        start_node_it = std::next(start_node_it);
    }

    // Initializes start_neigh_map for each dep to point to the first elem in graph.neigh_list[dep]
    void set_start_neigh_map(){
        for (const auto& [dep, neigh_list] : graph.neigh_list)
            start_neigh_map.emplace(dep, neigh_list.begin());
    }

    // Updates start_neigh_map[node] such that it points to the first node that is greater than start_node_it
    // node should already be in start_neigh_map
    std::optional<NodeChainRangeT> get_and_update_neigh_list_range(NodeConstItT node) {
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

    NodeConstItT get_real_nodes_start() const{
        return graph.abs_deps_map.begin();
    }

    NodeConstItT get_sentinel_node() const{
        return graph.abs_deps_map.end();
    }

    // Returns the end of the nodes container
    NodeConstItT get_nodes_end() const{
        return graph.abs_deps_map.end();
    }

    // Returns the end of the neighbour list of dep
    std::optional<NodeChainConstItT> get_neigh_list_end(NodeConstItT dep) const{
        auto neigh_list_it = graph.neigh_list.find(dep);
        if (neigh_list_it == graph.neigh_list.end())
            return {};

        return neigh_list_it->second.end();
    }

    bool empty() const{
        return start_node_it == get_nodes_end();
    }
};