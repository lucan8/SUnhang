#include <fstream>
#include <string>
#include <vector>
#include "../include/logger.hpp"
#include "../include/util.hpp"

int conv_trace(std::ifstream& in_trace_file, const std::string& out_trace_file_path){
    std::string evt_str;
    int line_index = 0;

    std::FILE* out_trace_file(std::fopen(out_trace_file_path.c_str(), "w"));
    if (!out_trace_file){
        Logger::print(LogType::ERR, "Extra log file not found: {}", out_trace_file_path);
        return 1;
    }

    while(std::getline(in_trace_file, evt_str)) {
        line_index++;
        std::vector<std::string> evt_str_split = split(evt_str, trace_sep);

        if(evt_str_split.size() != exp_split_trace_size) {
            Logger::print(LogType::WARN, "Bad file format on line {}: {}", line_index, evt_str);
            continue;
        }

        if (evt_str_split[1].find("acq") != std::string::npos || evt_str_split[1].find("rel") !=  std::string::npos){
            Logger::print(out_trace_file, "{}|{}|{}", evt_str_split[0], evt_str_split[1], evt_str_split[2]);
        }
    }

    return 1;
}

int main(int argc, char* argv[]){
    const uint8_t exp_args = 3;
    if (argc != exp_args){
        Logger::print(LogType::ERR, "Usage: ./conv_trace.exe [in_trace_path] [out_trace_path]");
        return 1; 
    }

    std::string in_file_path = argv[1];
    std::string out_file_path = argv[2];

    Logger::print(LogType::DBG, "Input path: {}", in_file_path);
    Logger::print(LogType::DBG, "Out file path: {}", out_file_path);

    std::ifstream in_file(in_file_path);
    if(!in_file.good()) {
        Logger::print(LogType::ERR, "In file not found: {}", in_file_path);
        return 1;
    }

    conv_trace(in_file, out_file_path);
}