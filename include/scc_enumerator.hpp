#pragma once

#include <map>
#include "scc_enumerator_types.hpp"

// Helper class that takes a dependency graph and gives the strongly connected component that has the smallest node
struct SCCEnumerator{
    // Read-only graph
    const std::map<AbsDependency, std::vector<VectorClock>>& abs_deps_map; // Nodes, they need to be sorted
    const std::unordered_map<const AbsDependency*, std::vector<const AbsDependency*>>& neigh_list; // Edges

    // Structure holding metadata about each node that is needed by the get_min_strong_conn_comp functions
    std::unordered_map<const AbsDependency*, AbsDepInfo> dep_info_map;
    
    // Also needed by get_min_strong_conn_comp
    int max_index;
    std::vector<const AbsDependency*> stack;

    // The strongest connected component that contains the minimum node
    MinSCC min_scc;

    // for debugging
    std::vector<MinSCC> scc_vec;

    SCCEnumerator(const std::map<AbsDependency, std::vector<VectorClock>>& abs_deps_map, const std::unordered_map<const AbsDependency*, std::vector<const AbsDependency*>>& neigh_list)
        : abs_deps_map(abs_deps_map), neigh_list(neigh_list){}

    // Returns the SCC with the smallest node overall
    MinSCC get_min_strong_conn_comp();

    // Recursive function that is called on each unvisited dependency
    // This is the implementation of tarjan's algorithm for SCC enumeration modified to return the component with the minimum id 
    void _get_min_strong_conn_comp(const AbsDependency* dep);

    // Should only be called after get_min_strong_conn_comp, prints information about the results
    void print_info() const;
};