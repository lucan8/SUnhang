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
    std::string show() const;
};

template <>
struct std::formatter<MinSCC> : std::formatter<std::string> {
    auto format(const MinSCC& scc, format_context& ctx) const {
        std::string result;

        for (const auto& node : scc.nodes)
          result += node->show() + "\n";
          
        result += "Min node: " + scc.min_node->show();
        
        return formatter<std::string>::format(result, ctx);
    }
};

inline std::string MinSCC::show() const{
    return std::format("{}", *this);
}