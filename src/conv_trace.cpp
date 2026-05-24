#include <fstream>
#include <string>
#include <vector>
#include "../include/logger.hpp"
#include "../include/util.hpp"
#include "../include/trace_parser.hpp"

int main(int argc, char* argv[]){
    const uint8_t exp_args = 3;
    if (argc != exp_args){
        Logger::print(LogType::ERR, "Usage: ./conv_trace.exe [in_std_trace_path] [out_bin_trace_path]");
        return 1; 
    }

    std::string in_file_path = argv[1];
    std::string out_file_path = argv[2];

    Logger::print(LogType::DBG, "Input path: {}", in_file_path);
    Logger::print(LogType::DBG, "Output path: {}", out_file_path);

    std::FILE* in_file(std::fopen(in_file_path.c_str(), "r"));
    if(!in_file) {
        Logger::print(LogType::ERR, "In file not found: {}", in_file_path);
        return 1;
    }

    std::FILE* out_file(std::fopen(out_file_path.c_str(), "wb"));
    if (!out_file){
        Logger::print(LogType::ERR, "Out file not found: {}", out_file_path);
        return 1;
    }

    StdParser std_parser(in_file);
    std_parser.to_bin_fmt(out_file);
}