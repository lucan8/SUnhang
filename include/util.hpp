#pragma once

#include <vector>
#include <string>
#include <sstream>
#include <cstdint>
#include <deque>
#include <type_traits>
#include <concepts>
#include <array>
#include "comm_types.hpp"

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

// res_id should be the id of either a lock or a cond_var
// for locks it returns the id of the associated cond var, for cond var the associated lock
inline ResourceIdT get_ass_sync_obj(ResourceIdT res_id){
    return -res_id;
}

// Returns true if ls1 and ls2 intersect, false otherwise
// Currently iterates of ls2 and checks if all elements are in ls1
inline bool lockset_intersection(const LocksetT& ls1, const LocksetT& ls2){
    for (const auto lock : ls2)
        if (ls1.find(lock) != ls1.end())
            return true;
    return false;
}

// Define constraint for LazyQueue that enforces the container to be either a vector or deque
template <typename> struct is_vec_or_deq : std::false_type{};
template <typename T, typename A> struct is_vec_or_deq<std::vector<T, A>> : std::true_type{};
template <typename T, typename A> struct is_vec_or_deq<std::deque<T, A>> : std::true_type{};

template <typename C>
concept is_vec_or_deq_c = is_vec_or_deq<C>::value;

// Queue that instead of removing front elements only moves start_elem to the right
// For this to make sense, the internal container should be sorted(and of course T should be comparable)
// The internal container can be either the const view of an existing container or individual one 
template <typename ContainerT, bool is_view>
requires is_vec_or_deq_c<ContainerT>
struct LazyQueue {
    // Extract the element type directly from the container
    using T = typename ContainerT::value_type;

    // StorageT is either a read only view or a self-standing container
    using StorageT = std::conditional_t<is_view, const ContainerT*, ContainerT>;
    
    StorageT queue; 
    ContainerT::const_iterator start_elem;

    // Individual container
    LazyQueue() requires (!is_view) : queue(ContainerT{}) {}

    // Read only view
    LazyQueue(const ContainerT& external) requires is_view : queue(&external) {reset();}

    // FUNCTIONS FOR NON-VIEW CONTAINER
    
    void push(const T& x) requires (!is_view) {
        queue.push_back(x);
    }

    template< class... Args >
    T& emplace(Args&&... args) requires (!is_view){
        return queue.emplace_back(std::forward<Args>(args)...);
    }

    T& back() requires (!is_view){
        return queue.back();
    }


    // COMMON FUNCTIONS

    const ContainerT& get() const {
        if constexpr (is_view) {
            return *queue;  // Dereference the pointer
        } else {
            return queue;   // Return the object directly
        }
    }
    
    // Returns the first element and pops
    // If "there are no more elements" the last will be returned
    const T* pop() {
        if (start_elem != queue.end()) {
            T* res = &(*start_elem);
            ++start_elem;
            return res;
        }
        return &back();
    }

    const T& back() const{
        return get().back();
    }

    // Pops all elements that are smaller than x and returns the last element poped
    // If inclusive is true, popping stops at the first element greater than x
    // If all elements are greater than x optional won't have a value
    template <typename ValT, typename CompT>
    requires std::predicate<CompT, const T&, const ValT&>
    std::optional<const T*> pop_until(const ValT& x, CompT comp, bool inclusive) {
        const ContainerT& q = get();

        if (inclusive){
            start_elem = std::upper_bound(start_elem, q.cend(), x, comp);
        }
        else{
            start_elem = std::lower_bound(start_elem, q.cend(), x, comp);
        }
        
        if (start_elem == q.begin())
            return {};
        
        return &(*std::prev(start_elem));
    }

    bool empty() const{
        return start_elem == get().cend();
    }

    size_t size() const{
        return get().size();
    }

    // Sets start_elem to the beginning of the internal container
    void reset() {
        start_elem = get().cbegin();
    }
    
};

// Aliases for LazyQueue
template <typename ContainerT>
using OwnedLazyQueue = LazyQueue<ContainerT, false>;

template <typename ContainerT>
using ViewLazyQueue = LazyQueue<ContainerT, true>;

template <typename ElemT, size_t Size>
struct CircularArr{
    std::array<ElemT, Size> _arr;
    size_t _start;
    size_t _size;

    using iterator = typename std::array<ElemT, Size>::iterator;
    using const_iterator = typename std::array<ElemT, Size>::const_iterator;

    CircularArr(): _start(0), _size(0){}

    void push(const ElemT& elem){
        // If the array is full use begin as both the start and end
        if (_size == Size){
            _arr[_start] = elem;
            _start = (_start + 1) % Size;
        } else{ // Otherwise use begin + _size as the end
            _arr[_start + _size] = elem;
            _size += 1;
        }
    }

    // Iterator to the first elements
    const_iterator begin() const{
        if (_size == 0)
            return _arr.end();
            
        return _arr.begin() + _start;
    }

    // Iterator to the last element(not past it!)
    // If the array is empty it is just begin
    const_iterator last() const{
        // Empty array, return begin
        if (_size == 0)
            return _arr.begin();

        // Not full, use size as the end
        if (_size < Size)
            return _arr.begin() + _size - 1;
        
        // Full and at the start, last is the real last
        if (_start == 0)
            return _arr.end() - 1;
        
        return begin() - 1;
    }

    // Iterator to an invalid element outside of the array(should be used as an anchor)
    const_iterator end() const{
        return _arr.end();
    }

    // Returns the next iterator after curr. Returns end when the last element is reached
    const_iterator next(const_iterator curr) const{
        auto last_it = last();
        // Finish when reaching last element
        if (curr == last_it){
            return end();
        }

        // Go to the next in a circular manner
        const_iterator res = curr + 1;
        if (res == end())
            return _arr.begin();
        
        return res;
    }

    void merge_into(const CircularArr& other){
        for (auto it = other.begin(); it != other.end(); it = other.next(it)){
            this->push(*it);
        }
    }
};

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