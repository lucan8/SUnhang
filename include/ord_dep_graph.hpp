#pragma once

#include <map>
#include <ranges>
#include <optional>

#include "predictor_types.hpp"

typedef std::vector<NodeConstItT> NodeChainT;
typedef std::unordered_set<NodeConstItT, IteratorHasher> NodeUSetT;
typedef std::vector<NodeConstItT>::const_iterator NodeChainConstItT;
typedef std::ranges::subrange<NodeChainConstItT> NodeChainRangeT;

// typedef std::unordered_set<NodeConstItT, IteratorHasher, IteratorHasher> NodeSetT;

typedef std::unordered_map<NodeConstItT, NodeChainT, IteratorHasher, IteratorHasher> NeighListT;

// Format for NodeChainT
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
// sentinel_node should be a valid sentinel for both nodes otherwise you will undefined behaviour
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

    // Map that gives the valid start neighbour of each node
    std::unordered_map<NodeConstItT, NodeChainConstItT, IteratorHasher> start_neigh_map;

    // Initializes start_node_it and start_neigh_map
    void init_start_structs();

    // Initializes start_node_it to point to the first node in graph.abs_dep_map
    void set_start_node();

    // Initializes start_node_it to point to node
    void set_start_node(NodeConstItT node);

    // std::next(start_node_it)
    void advance_start_node();

    // Initializes start_neigh_map for each dep to point to the first elem in graph.neigh_list[dep]
    void set_start_neigh_map();

    // Updates start_neigh_map[node] such that it points to the first node that is greater than start_node_it
    // node should already be in start_neigh_map
    std::optional<NodeChainRangeT> get_and_update_neigh_list_range(NodeConstItT node);

    // Returns the start of the node container, ignoring start_node_it
    NodeConstItT get_real_nodes_start() const;

    // Returns the end of the node container of the graph
    NodeConstItT get_sentinel_node() const;

    // Returns the end of the nodes container
    NodeConstItT get_nodes_end() const;

    // Returns the end of the neighbour list of dep
    std::optional<NodeChainConstItT> get_neigh_list_end(NodeConstItT dep) const;

    // Returns true if start_node_it == node container end
    bool empty() const;
};