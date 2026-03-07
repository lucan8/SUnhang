#pragma once

#include <vector>
#include <string>
#include <sstream>
#include <cstdint>
#include <deque>
#include "comm_types.hpp"

const char trace_sep = '|';
const uint8_t exp_split_trace_size = 3;

// Splits str by sep
inline std::vector<std::string> split(const std::string& str, char sep){
    std::stringstream ss(str);
    std::vector<std::string> result;

    while(ss.good()) {
        std::string substring;
        getline(ss, substring, sep);
        result.push_back(substring);
    }

    return result;
}

// Returns true if ls1 and ls2 intersect, false otherwise
// Currently iterates of ls2 and checks if all elements are in ls1
inline bool lockset_intersection(const LocksetT& ls1, const LocksetT& ls2){
    for (const auto lock : ls2)
        if (ls1.find(lock) != ls1.end())
            return true;
    return false;
}

// Queue that instead of removing front elements only moves start_elem to the right
// For this to make sense, the internal container should be sorted(and of course T should be comparable)
// The deque was chosen for fast pushes/accces to the front and back, binary search 
// and most importantly pointer stability!
template <typename T>
struct LazyQueue {
    std::deque<T> queue;
    std::deque<T>::const_iterator start_elem;

    void push(const T& x) {
        queue.push_back(x);
    }

    template< class... Args >
    T& emplace(Args&&... args) {
        return queue.emplace_back(std::forward<Args>(args)...);
    }

    void pop() {
        if (start_elem != queue.end()) {
            ++start_elem;
        }
    }

    T& back() {
        return queue.back();
    }

    const T& back() const{
        return queue.back();
    }


    void pop_until(const T& x) {
        start_elem = std::lower_bound(start_elem, queue.end(), x);
    }

    bool empty() const{
        return queue.empty();
    }
};

// Generic struct used for pointer to object comparison
// Note: Nullptr is treated as infinity
struct PtrLess {
    template <typename T>
    bool operator()(const T* a, const T* b) const {
        if (a == b) 
            return false;
        
        // a=nullptr, b=val -> false, a=val, b=nullptr->true, made like this to help min!
        if (!a || !b) 
            return a > b;
        
        return *a < *b;
    }
};

template <typename Iter>
bool is_valid_iter(Iter iter, Iter sentinel){
    return iter != sentinel;
}

struct IteratorHasher {
    template <typename Iter>
    std::size_t operator()(const Iter& it) const {
        // Hash the address of the pair the iterator points to
        return std::hash<const void*>{}(&(*it));
    }

    // Add this to handle the equality check
    template <typename Iter>
    bool operator()(const Iter& lhs, const Iter& rhs) const {
        return lhs == rhs;
    }
};