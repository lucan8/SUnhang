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
#include "logger.hpp"

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

inline bool is_cond_var(ResourceIdT res_id){
    return res_id < 0;
}

// Returns true if ls1 and ls2 intersect, false otherwise
// Currently iterates of ls2 and checks if all elements are in ls1
inline bool lockset_intersection(const LocksetT& ls1, const LocksetT& ls2){
    for (const auto lock : ls2)
        if (ls1.find(lock) != ls1.end())
            return true;
    return false;
}

// Returns true if ls1 and ls2 intersect, false otherwise
// Doesn't count cond vars for intersection
inline bool lockset_intersection_soft(const LocksetT& ls1, const LocksetT& ls2){
    for (const auto lock : ls2){
        auto found_elem = ls1.find(lock);
        if (found_elem != ls1.end() && !is_cond_var(*found_elem))
            return true;
    }
    return false;
}

// Insert the elements of src in dst ignoring cond vars
// Returns true if no duplicates were found false otherwise, stopping at the first found element
inline bool insert_lockset(const LocksetT& src, ULocksetT& dst){
    for (const auto& res_id : src){
        if (!is_cond_var(res_id)){ // Ignore cond vars
            auto [it, inserted] = dst.insert(res_id);
            if (!inserted){
                return false;
            }
        }
    }

    return true;
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

// Note: All those functions have the weird template to take into consideration both iterator and const_iterator
// TODO: Could use a fixed size vector instead of array to be more compact
template <typename ElemT, size_t Size>
struct CircularArr{
    std::array<ElemT, Size> _arr;
    size_t _start;
    size_t _size;

    using iterator = typename std::array<ElemT, Size>::iterator;
    using const_iterator = typename std::array<ElemT, Size>::const_iterator;

    CircularArr(): _start(0), _size(0){}

    // Push elem into _arr and returns the overwritten element(if any)
    std::optional<ElemT> push(const ElemT& elem){
        // If the array is full use begin as both the start and end
        if (_size == Size){
            ElemT ret = unsafe_set(_start, elem);
            _start = (_start + 1) % Size;
            return ret;
        }

        // Otherwise use begin + _size as the end
        _arr[_start + _size] = elem;
        _size += 1;
        return {};
    }

    // Assumes pos is valid
    ElemT unsafe_set(size_t pos, const ElemT& elem) {
        ElemT old_value = std::move(_arr[pos]);
        _arr[pos] = elem;
        
        return old_value;
    }

    // Assumes pos is valid
    ElemT unsafe_set(const_iterator pos, const ElemT& elem) {
        size_t index = std::distance(_arr.cbegin(), pos);
        ElemT old_value = std::move(_arr[index]);

        _arr[index] = elem;
        
        return old_value;
    }

    bool is_valid_iter(const_iterator pos) const{
        return pos >= _arr.cbegin() && pos < end();
    }

    std::optional<ElemT> set(const_iterator pos, const ElemT& elem) {
        if (!is_valid_iter(pos)){
            return {};
        }

        return unsafe_set(pos, elem);
    }

    // Returns the next iterator after curr. Returns end when the last element is reached
    template <typename Self>
    auto next(this Self&& self, decltype(self.begin()) curr) {
        auto last_it = self.last();
        
        // Finish when reaching last element
        if (curr == last_it) {
            return self.end();
        }

        // Go to the next in a circular manner.
        auto res = curr + 1; 
        
        if (res == self.end())
            return self._arr.begin();
        
        return res;
    }

    template <typename Self>
    auto begin(this Self&& self) {
        if (self._size == 0)
            return self.end();
            
        return self._arr.begin() + self._start;
    }

    template <typename Self>
    auto last(this Self&& self) {
        // Empty? return begin
        if (self._size == 0)
            return self._arr.begin();

        // Not full? use size to determine the last element
        if (self._size < Size)
            return self._arr.begin() + self._size - 1;
        
        // Full and _start it "at the real start"? Return the "real last element"
        if (self._start == 0)
            return self._arr.end() - 1;
            
        return self.begin() - 1;
    }

    template <typename Self>
    auto end(this Self&& self) {
        return self._arr.begin() + self._size;
    }

    bool contains(const ElemT& elem) const{
        for (auto it = begin(); it != end(); it = next(it)){
            if (*it == elem){
                return true;
            }
        }
        return false;
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