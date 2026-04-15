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

// Fixed size, double ended circular linked list
template <typename T, size_t Capacity>
struct CircularList {
    struct Node {
        std::optional<T> data;
        int prev = -1;
        int next = -1;
    };

    template <bool IsConst>
    struct IteratorBase {
        using NodePtrT = std::conditional_t<IsConst, const Node*, Node*>;
        NodePtrT _ptr;
        NodePtrT _base_ptr; // Used to transform indexes to pointers

        using PtrT = std::conditional_t<IsConst, const Node*, Node*>;
        using RefT = std::conditional_t<IsConst, const Node&, Node&>;
        
        IteratorBase(Node* base_ptr, Node* ptr = nullptr) : _base_ptr(base_ptr), _ptr(ptr){}
        IteratorBase(Node* base_ptr, int idx = -1) : _base_ptr(base_ptr){
            _ptr = ind_to_ptr(idx);
        }

        // Conversion from iterator to const_iterator
        template <bool WasConst, typename = std::enable_if_t<IsConst && !WasConst>>
        IteratorBase(const IteratorBase<WasConst>& other) : _ptr(other._ptr) {}

        NodePtrT ind_to_ptr(int index){
            NodePtrT res = index == -1 ? nullptr : _base_ptr + index;
            return res;
        }

        int to_ind(){
            return _ptr - _base_ptr;
        }
        
        RefT& operator*() {return *_ptr;}
        PtrT operator->() const {return _ptr;}
        IteratorBase& operator++() {_ptr = ind_to_ptr(_ptr->next); return *this;}
        bool operator!=(const IteratorBase& other) const {return _ptr != other._ptr;}
        bool operator==(const IteratorBase& other) const {return _ptr == other._ptr;}
    };

    using iterator = IteratorBase<false>;
    using const_iterator = IteratorBase<true>;
    
    std::array<Node, Capacity> nodes;
    
    int head = -1;      // Points to the oldest element
    int tail = -1;      // Points to the newest element
    int free_head = 0;  // Points to the first available empty slot
    size_t size = 0;

    CircularList() {
        // Initialize the free list to chain all empty slots together
        for (int i = 0; i < Capacity - 1; ++i) {
            nodes[i].next = i + 1;
        }
        nodes[Capacity - 1].next = -1;
    }

    // Insert at the end. If full, overwrites the oldest element
    // Returns the erased element's value and the index of the inserted element
    std::pair<std::optional<T>, int> push(const T& value) {
        int new_node_idx;
        std::optional<T> erased_elem;

        if (full()) {
            erased_elem = nodes[head].data;
            new_node_idx = head;
            
            head = nodes[head].next;
            nodes[head].prev = -1;
            
            size--;
        } else {
            // Pop an empty slot from the free list
            new_node_idx = free_head;
            free_head = nodes[free_head].next;
        }

        // Setup the new node's data and links
        nodes[new_node_idx].data = value;
        nodes[new_node_idx].next = -1;
        nodes[new_node_idx].prev = tail;

        if (tail != -1) {
            nodes[tail].next = new_node_idx;
        }
        tail = new_node_idx;

        // If this is the first element, it's also the head
        if (head == -1) {
            head = new_node_idx;
        }

        size++;
        return {erased_elem, new_node_idx};
    }

    // Remove from anywhere using index
    // Doesn't check for a valid index
    T unsafe_erase(int target_idx) {
        Node& target = nodes[target_idx];

        // Unlink from the active list
        if (target.prev != -1) {
            nodes[target.prev].next = target.next;
        } else {
            head = target.next;
        }

        if (target.next != -1) {
            nodes[target.next].prev = target.prev;
        } else {
            tail = target.prev;
        }

        T result = std::move(target.data.value());

        // Clear the data and push the slot back onto the free list
        target.data.reset();
        target.prev = -1;
        target.next = free_head;
        free_head = target_idx;

        size--;
        
        // Reset list if it becomes empty
        if (empty()) {
            head = -1;
            tail = -1;
        }

        return result;
    }

    // Remove from anywhere using iterator
    // Doesn't check for a valid iterator
    T unsafe_erase(iterator target_it) {
        int target_idx = target_it.to_ind();
        return unsafe_erase(target_idx);
    }

    // Makes the element at target_idx be the newest
    // Assumes target_idx is valid
    void unsafe_update(int target_idx){
        // Updating the last element doesn't do anything
        if (target_idx == tail){
            return;
        }

        Node& target = nodes[target_idx];

        // Set target's neighbours pointers
        if (target_idx == head){
            head = target.next;
        }
        else{
            nodes[target.prev].next = target.next;
        }

        nodes[target.next].prev = target.prev;
        
        // Set target pointers
        target.next = -1;
        target.prev = tail;

        // Set the tail
        nodes[tail].next = target_idx;
        tail = target_idx;
    }

    template <typename Self>
    auto begin(this Self&& self) {
        constexpr bool is_const = std::is_const_v<std::remove_reference_t<Self>>;
        return IteratorBase<is_const>(self.nodes.data(), self.head); 
    }

    template <typename Self>
    auto end(this Self&& self) {
        constexpr bool is_const = std::is_const_v<std::remove_reference_t<Self>>;
        return IteratorBase<is_const>(self.nodes.data(), nullptr); 
    }

    template <typename Self>
    auto last(this Self&& self) {
        constexpr bool is_const = std::is_const_v<std::remove_reference_t<Self>>;
        return IteratorBase<is_const>(self.nodes.data(), self.tail); 
    }

    bool full() const{
        return size == Capacity;
    }

    bool empty() const{
        return size == 0;
    }
};

// Fixed size, circular LRU cache
template <typename T, size_t Capacity, typename Hash>
struct CircularLRU{
    using container_t = CircularList<T, Capacity>;
    using iterator = container_t::iterator;
    using const_iterator = container_t::const_iterator;

    container_t container;
    std::unordered_map<T, int, Hash> idx_umap;

    void push(const T& value){
        // Update if element already present
        auto value_idx_it = idx_umap.find(value);
        if (value_idx_it != idx_umap.end()){
            container.unsafe_update(value_idx_it->second);
            return;
        }

        // Push new
        auto [old_val_opt, new_val_ind] = container.push(value);
        idx_umap[value] = new_val_ind;
        
        // Erase old val from map
        if (old_val_opt.has_value()){
            idx_umap.erase(old_val_opt.value());
        }
    }

    // Updates the value pointed by target_it to be new_value
    // Assumes target_it is a valid iterator
    void unsafe_set(iterator target_it, const T& new_value){
        int target_idx = target_it.to_ind();
        unsafe_set(target_idx, new_value);
    }

    // Updates the value pointed by target_idx to be new_value
    // Assumes target_idx is a valid index
    void unsafe_set(size_t target_idx, const T& new_value){
        auto& target = container.nodes[target_idx];

        // Set the new value
        T old_value = std::move(target.data.value());
        target.data = new_value;

        // Update the index map
        idx_umap.erase(old_value);
        idx_umap.insert({new_value, target_idx}); 
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