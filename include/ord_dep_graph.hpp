#pragma once

#include <map>
#include <cassert>

#include "predictor_types.hpp"


// Struct that just holds together nodes and edges
struct OrdDepGraph{
    // AbsDependency represents a node the graph
    std::map<AbsDependency, std::vector<VectorClock>> abs_deps_map;

    // The vector in neigh list is ordered
    std::unordered_map<const AbsDependency*, std::vector<const AbsDependency*>> neigh_list;
};

// Struct that exposes a view on a graph
struct OrdDepGraphView{
    OrdDepGraph graph;

    // Reference to the current valid node in the graph
    std::map<AbsDependency, std::vector<VectorClock>>::const_iterator start_node_it;

    // Map that gives the start neighbour of each node
    std::unordered_map<const AbsDependency*, std::vector<const AbsDependency*>::const_iterator> start_neigh_map;

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

    // Returns the end of the nodes container
    std::map<AbsDependency, std::vector<VectorClock>>::const_iterator get_nodes_end() const{
        return graph.abs_deps_map.end();
    }

    // Returns the end of the neighbour list of dep
    std::vector<const AbsDependency*>::const_iterator get_neigh_list_end(const AbsDependency* dep) const{
        auto neigh_list_it = graph.neigh_list.find(dep);
        assert(neigh_list_it != graph.neigh_list.end()); // How did you manage to pass an invalid dependency?

        return neigh_list_it->second.end();
    }

};