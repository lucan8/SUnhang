#include "../include/logger.hpp"
#include <cstdio>
#include <cstdarg>

void Logger::print(LogType log_type, const char* format, ...) {    
    _print_log_type(log_type);

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    
    printf("\n");
}

void Logger::_print_log_type(LogType log_type){
    switch (log_type){
        case LogType::INFO:
            printf("[INFO]:");
            break;
        case LogType::WARN:
            printf("[WARN]:");
            break;
        case LogType::ERR:
            printf("[ERR]:");
            break;
        case LogType::DBG:
            printf("[DBG]:");
            break;
        default:
            break;
    }
}