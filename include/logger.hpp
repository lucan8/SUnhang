#pragma once

#include <format>
#include <string_view>
#include <print>

enum class LogType{        
    INFO,
    WARN,
    ERR,
    DBG,
    NONE
};

struct Logger {
    // Wrapper over print that prepends the output with stuff like [ERR]:, [WARN]: etc...
    template<typename... Args>
    static void print(LogType log_type, std::format_string<Args...> fmt, Args&&... args){
        _print_log_type(log_type);

        std::print(fmt, std::forward<Args>(args)...);
        
        std::print("\n");
    }

    // Wrapper over print that prepends the output with stuff like [ERR]:, [WARN]: etc...
    template<typename... Args>
    static void print(std::FILE* stream, std::format_string<Args...> fmt, Args&&... args){
        std::print(stream, fmt, std::forward<Args>(args)...);
        
        std::print(stream, "\n");
    }

    static void print_dash_line();
    static void _print_log_type(LogType log_type);
};