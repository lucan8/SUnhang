#pragma once

#include <map>
#include <cassert>

#include "predictor_types.hpp"

typedef std::map<AbsDependency, std::vector<VectorClock>> NodeContainerT;
typedef std::map<AbsDependency, std::vector<VectorClock>>::const_iterator NodeConstItT;

typedef std::vector<NodeConstItT> NodeChainT;
typedef std::vector<NodeConstItT>::const_iterator NodeChainConstItT;

typedef std::unordered_map<NodeConstItT, NodeChainT, IteratorHasher> NeighListT;

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
    std::unordered_map<NodeConstItT, NodeChainConstItT, IteratorHasher>  start_neigh_map;

    // Initializes start_node_it and start_neigh_map
    void init_start_structs(){
        set_node_start();
        set_neigh_list_start();
    }

    // Initializes start_node_it to point to the first node in graph.abs_dep_map
    void set_node_start(){
        start_node_it = graph.abs_deps_map.begin();
    }

    // Initializes start_neigh_map for each dep to point to the first elem in graph.neigh_list[dep]
    void set_neigh_list_start(){
        for (const auto& [dep, neigh_list] : graph.neigh_list)
            start_neigh_map.emplace(dep, neigh_list.begin());
    }

    NodeConstItT get_real_nodes_start() const{
        return graph.abs_deps_map.begin();
    }

    // Returns the end of the nodes container
    NodeConstItT get_nodes_end() const{
        return graph.abs_deps_map.end();
    }

    // Returns the end of the neighbour list of dep
    NodeChainConstItT get_neigh_list_end(NodeConstItT dep) const{
        auto neigh_list_it = graph.neigh_list.find(dep);
        assert(neigh_list_it != graph.neigh_list.end()); // How did you manage to pass an invalid dependency?

        return neigh_list_it->second.end();
    }
};