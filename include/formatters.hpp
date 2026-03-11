#include <format>
#include "predictor_types.hpp"
#include "util.hpp"

// Formatter for event, only formats trace position
template <>
struct std::formatter<Event> : std::formatter<std::string> {
  
  auto format(const Event& ev, auto& ctx) const {
      return std::format_to(ctx.out(), "{}", ev.tr_pos);
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

// Format for LazyQueue, only formats start_elem
template <typename ContainerT, bool is_view>
struct std::formatter<LazyQueue<ContainerT, is_view>> :  std::formatter<std::string> {
    auto format(const LazyQueue<ContainerT, is_view>& lazy_queue, auto& ctx) const {
        return std::format_to(ctx.out(), "{}", *lazy_queue.start_elem);
    }
};