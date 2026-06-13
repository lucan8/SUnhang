#include <fstream>
#include <string>
#include <vector>
#include <bit>
#include <cstdio>
#include <ranges>
#include "../include/logger.hpp"
#include "../include/std_trace_parser.hpp"
#include "../include/meta_info.hpp"

void java_to_local_enc(std::FILE* src_file, std::FILE* dst_file){
    // Load header and swap the binary encoding
    MetaHeader meta_header;
    meta_header.load(src_file);
    meta_header.byteswap();

    std::vector<BinEvT> java_enc_events(meta_header.EVENT_COUNT);
    std::vector<BinEvT> local_enc_events;
    local_enc_events.reserve(meta_header.EVENT_COUNT);

    std::fread(java_enc_events.data(), sizeof(BinEvT), java_enc_events.size(), src_file);

    // Swap the encoding and ignore invalid events
    for (const auto& ev : java_enc_events){
        BinEvT ev_swapped = std::byteswap(ev);
        auto ev_type_opt = TraceBinFormatter::extract_ev_type(ev_swapped);
        if (ev_type_opt.has_value() && !is_unused_type(ev_type_opt.value())){
            local_enc_events.push_back(ev_swapped);
        }
        // else{
        //     Logger::print(LogType::DBG, "Skipping event: {}", static_cast<int16_t>(ev_type_opt.value()));
        // }
    }

    // Account for ignored events
    meta_header.EVENT_COUNT = local_enc_events.size();

    // Print the header
    meta_header.store(dst_file);
   
    // Print binary events. 
    // If the type is smaller than 8 bytes, print them one by one to avoid printing the padding too
    std::fwrite(local_enc_events.data(), sizeof(local_enc_events[0]), local_enc_events.size(), dst_file);
}

void local_to_java_enc(std::FILE* src_file, std::FILE* dst_file){
    // Load header
    MetaHeader meta_header;
    meta_header.load(src_file);

    std::vector<BinEvT> java_enc_events(meta_header.EVENT_COUNT);
    std::vector<BinEvT> local_enc_events;
    local_enc_events.reserve(meta_header.EVENT_COUNT);

    std::fread(java_enc_events.data(), sizeof(BinEvT), java_enc_events.size(), src_file);

    // Swap the encoding and ignore invalid events
    for (const auto& ev : java_enc_events){
        auto ev_type_opt = TraceBinFormatter::extract_ev_type(ev);
        if (ev_type_opt.has_value() && is_spd_type(ev_type_opt.value())){
            local_enc_events.push_back(std::byteswap(ev));
        }
        else{
            // Logger::print(LogType::DBG, "Skipping event: {}", static_cast<int16_t>(ev_type_opt.value()));
        }
    }

    // Account for ignored events
    meta_header.EVENT_COUNT = local_enc_events.size();

    // Print the header
    meta_header.byteswap();
    meta_header.store(dst_file);
   
    // Print binary events. 
    // If the type is smaller than 8 bytes, print them one by one to avoid printing the padding too
    std::fwrite(local_enc_events.data(), sizeof(local_enc_events[0]), local_enc_events.size(), dst_file);
}

void local_to_std_enc(std::FILE* src_file, std::FILE* dst_file){
    // Load header
    MetaHeader meta_header;
    meta_header.load(src_file);

    // Reads all events at once
    std::vector<BinEvT> java_enc_events(meta_header.EVENT_COUNT);
    std::fread(java_enc_events.data(), sizeof(BinEvT), java_enc_events.size(), src_file);

    // Swap the encoding, ignore invalid events and print in std format
    for (const auto& [idx, ev] : std::views::enumerate(java_enc_events)){
        auto java_ev_opt = TraceBinFormatter::bin_to_ev(ev);

        if (java_ev_opt.has_value()){
            std::string std_fmt_ev = std::format("{}\n", java_ev_opt.value());
            std::fwrite(std_fmt_ev.c_str(), sizeof(std_fmt_ev[0]), std_fmt_ev.size(), dst_file);
        }
        else{
            // Logger::print(LogType::DBG, "Skipping event: {}", idx);
        }
    }
}

int main(int argc, char* argv[]){
    const uint8_t exp_args = 5;
    if (argc != exp_args){
        Logger::print(LogType::ERR, "Usage: ./conv_trace.exe [in_std_trace_path] [out_bin_trace_path] [from_fmt] [to_fmt]");
        return 1; 
    }

    std::string in_file_path = argv[1];
    std::string out_file_path = argv[2];
    std::string from_fmt = argv[3];
    std::string to_fmt = argv[4];

    // Logger::print(LogType::INFO, "Converting from {} to {} format", from_fmt, to_fmt);

    // Convert from std to local binary encoding
    if (from_fmt == "std"){
        // Open output in write binary mode
        std::FILE* out_file(std::fopen(out_file_path.c_str(), "wb"));
        if (!out_file){
            Logger::print(LogType::ERR, "Out file not found: {}", out_file_path);
            return 1;
        }

        // Open input in read mode 
        std::FILE* in_file(std::fopen(in_file_path.c_str(), "r"));
        if(!in_file) {
            Logger::print(LogType::ERR, "In file not found: {}", in_file_path);
            std::fclose(out_file);
            return 1;
        }

        StdParser std_parser(in_file);
        std_parser.to_bin_fmt(out_file);

        std::fclose(in_file);
        std::fclose(out_file);
    }
    else if (from_fmt == "local_enc"){ 
        if (to_fmt == "std"){
            // Open file in read binary mode
            std::FILE* in_file(std::fopen(in_file_path.c_str(), "rb"));
            if(!in_file) {
                Logger::print(LogType::ERR, "In file not found: {}", in_file_path);
                return 1;
            }

            // Open out file in write mode
            std::FILE* out_file(std::fopen(out_file_path.c_str(), "w"));
            if(!out_file) {
                Logger::print(LogType::ERR, "Out file not found: {}", out_file_path);
                fclose(in_file);
                return 1;
            }

            local_to_std_enc(in_file, out_file);
            
            fclose(in_file);
            fclose(out_file);
        }
        else if(to_fmt == "java_enc"){
            // Copy file if encoding is big endian
            if constexpr (std::endian::native == std::endian::big) {
                std::ifstream src(in_file_path, std::ios::binary);
                std::ofstream dst(out_file_path, std::ios::binary);
                if (src && dst) {
                    dst << src.rdbuf();
                    return 0;
                }
                return 1;
            }

            // Open file in read binary mode
            std::FILE* in_file(std::fopen(in_file_path.c_str(), "rb"));
            if(!in_file) {
                Logger::print(LogType::ERR, "In file not found: {}", in_file_path);
                return 1;
            }

            // Open out file in write binary mode
            std::FILE* out_file(std::fopen(out_file_path.c_str(), "wb"));
            if(!out_file) {
                Logger::print(LogType::ERR, "Out file not found: {}", out_file_path);
                fclose(in_file);
                return 1;
            }

            local_to_java_enc(in_file, out_file);
            std::fclose(in_file);
        }
    }
    else if (from_fmt == "java_enc"){
        // TODO: Handle the case of BIG ENDIAN

        // Open file in read binary mode
        std::FILE* in_file(std::fopen(in_file_path.c_str(), "rb"));
        if(!in_file) {
            Logger::print(LogType::ERR, "In file not found: {}", in_file_path);
            return 1;
        }

        // Open out file in write binary mode
        std::FILE* out_file(std::fopen(out_file_path.c_str(), "wb"));
        if(!out_file) {
            Logger::print(LogType::ERR, "Out file not found: {}", out_file_path);
            fclose(in_file);
            return 1;
        }

        java_to_local_enc(in_file, out_file);
        std::fclose(in_file);
    }

    return 0;
}