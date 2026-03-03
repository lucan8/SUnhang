#pragma once

#include <unordered_set>
#include "predictor_types.hpp"
#include <format>

// Information about the dependency that is needed whilst computing the strongly connected component
struct AbsDepInfo{
    int index;
    int low_index;
    bool on_stack;

    AbsDepInfo(int index, int low_index, bool on_stack)
        : index(index), low_index(low_index), on_stack(on_stack){}
};


// Contains the strongly connected component as an unordered_set of nodes(subgraph) and a pointer to the minimum node 
struct MinSCC{
    std::unordered_set<const AbsDependency*> nodes;
    const AbsDependency* min_node;

    MinSCC(): nodes(), min_node(nullptr){}

    bool operator<(const MinSCC& other) const{
        return PtrLess()(min_node, other.min_node);
    }
};

template <>
struct std::formatter<MinSCC> : std::formatter<std::string> {
    auto format(const MinSCC& scc, format_context& ctx) const {
        auto out = ctx.out();

        for (const auto& node : scc.nodes) {
            out = std::format_to(out, "{}\n", *node);
        }
        
        return std::format_to(out, "Min node: {}", *scc.min_node);
    }
};