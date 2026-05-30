#pragma once

#include <algorithm>
#include <functional>
#include <vector>

template <typename T, typename Compare = std::less<T>>
struct SortedVector {
    std::vector<T> _vec;
    Compare _comp;

    SortedVector() = default;
    SortedVector(size_t size) : _vec(size){}
    
    auto operator<=>(const SortedVector& other) const {
        return _vec <=> other._vec;
    }

    bool operator==(const SortedVector& other) const {
        return _vec == other._vec;
    }
    
    bool contains(const T& val) const {
        auto it = std::lower_bound(_vec.begin(), _vec.end(), val, _comp);
        return it != _vec.end() && *it == val; 
    }

    auto find(const T& val) const {
        auto it = std::lower_bound(_vec.begin(), _vec.end(), val, _comp);
        if (it == _vec.end() || *it != val){
            return _vec.end();
        }
        return it;
    }

    void insert(const T& val){
        auto it = std::lower_bound(_vec.begin(), _vec.end(), val, _comp);
        _vec.insert(it, val);    
    }

    T& unique_insert(const T& val){
        auto it = std::lower_bound(_vec.begin(), _vec.end(), val, _comp);
        if (it == _vec.end() || *it != val){
            it = _vec.insert(it, val);
        }
        return *it;
    }
    
    bool erase(const T& val){
        auto it = std::lower_bound(_vec.begin(), _vec.end(), val, _comp);
        if (it != _vec.end() && *it == val) {
            _vec.erase(it);
            return true;
        }
        return false;
    }

    void unsafe_erase(const T& val){
        auto it = std::lower_bound(_vec.begin(), _vec.end(), val, _comp);
        _vec.erase(it);
    }
};