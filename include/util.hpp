#pragma once

#include <vector>
#include <string>
#include <sstream>

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


// Generic struct used for pointer comparison
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

// Formatter for pointers, just formats the object it point to
// Ignores void and char ptrs, makes sure the underlying type T is formatable
template <typename T>
requires (!std::same_as<std::remove_cv_t<T>, void>) &&  
         (!std::same_as<std::remove_cv_t<T>, char>) &&
         std::formattable<T, char>
struct std::formatter<T*> :  std::formatter<std::string>{
  
  auto format(const T* ptr, auto& ctx) const {
    if (!ptr) 
        return std::format_to(ctx.out(), "nullptr");
    return std::format_to(ctx.out(), "{}", *ptr);
  }
};


template <typename Iter>
bool is_valid_iter(Iter iter, Iter sentinel){
    return iter != sentinel;
}

struct IteratorHasher {
    // Uses the address the iterator point to
    template <typename Iter>
    std::size_t operator()(const Iter& it) const {
        return std::hash<const void*>{}(&(*it));
    }

    // Equality check
    template <typename Iter>
    bool operator()(const Iter& lhs, const Iter& rhs) const {
        return lhs == rhs;
    }
};