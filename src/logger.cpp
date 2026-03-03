#include "../include/logger.hpp"

void Logger::_print_log_type(LogType log_type){
    switch (log_type){
        case LogType::INFO:
            std::print("[INFO]:");
            break;
        case LogType::WARN:
            std::print("[WARN]:");
            break;
        case LogType::ERR:
            std::print("[ERR]:");
            break;
        case LogType::DBG:
            std::print("[DBG]:");
            break;
        default:
            break;
    }
}

void Logger::print_dash_line(){
    Logger::print(LogType::INFO, "------------------------------------");
}