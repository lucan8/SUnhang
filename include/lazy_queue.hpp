#pragma once

#include <vector>
#include <deque>

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
    std::pair<std::optional<const T*>, bool> pop_until(const ValT& x, CompT comp, bool inclusive) {
        const ContainerT& q = get();
        auto tmp = start_elem;

        if (inclusive){
            start_elem = std::upper_bound(start_elem, q.cend(), x, comp);
        }
        else{
            start_elem = std::lower_bound(start_elem, q.cend(), x, comp);
        }

        if (start_elem == q.begin())
            return {std::nullopt, false};
        
        return {&(*std::prev(start_elem)), tmp == start_elem};
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

// Format for LazyQueue, only formats start_elem
template <typename ContainerT, bool is_view>
struct std::formatter<LazyQueue<ContainerT, is_view>> :  std::formatter<std::string> {
    auto format(const LazyQueue<ContainerT, is_view>& lazy_queue, auto& ctx) const {
        return std::format_to(ctx.out(), "{}", *lazy_queue.start_elem);
    }
};

// Aliases for LazyQueue
template <typename ContainerT>
using OwnedLazyQueue = LazyQueue<ContainerT, false>;

template <typename ContainerT>
using ViewLazyQueue = LazyQueue<ContainerT, true>;