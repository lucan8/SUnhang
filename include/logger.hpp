#pragma once

enum class LogType{        
    INFO,
    WARN,
    ERR,
    DBG
};

struct Logger {
    // Wrapper over printf that prepends the output with stuff like [ERR]:, [WARN]: etc...
    // __attribute__ format makes sure we still see the compiler format warnings printf would've had
    // 2 and 3 represent the argument indexes for format and args for printf
    __attribute__((format(printf, 2, 3)))
    static void print(LogType log_type, const char* format, ...);

    static void _print_log_type(LogType log_type);
};