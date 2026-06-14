#include "../include/logger.hpp"

void Logger::_print_log_type(LogType log_type, std::FILE* out_file){
    out_file = (out_file != nullptr) ? out_file : stdout;

    switch (log_type){
        case LogType::INFO:
            std::print(out_file, "[INFO]:");
            break;
        case LogType::WARN:
            std::print(out_file, "[WARN]:");
            break;
        case LogType::ERR:
            std::print(out_file, "[ERR]:");
            break;
        case LogType::DBG:
            std::print(out_file, "[DBG]:");
            break;
        default:
            break;
    }
}

void Logger::print_dash_line(std::FILE* out_file){
    if (!out_file)
        Logger::print(LogType::NONE, "------------------------------------");
    else
        Logger::print(out_file, "------------------------------------");
}