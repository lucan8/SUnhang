#pragma once

#include <bit>
#include <cstdio>

#include "common_types.hpp"

struct MetaHeader{
    ThreadIdT THREAD_COUNT = 0;
    ResourceIdT LOCK_COUNT = 0;
    ResourceIdT VAR_COUNT = 0;
    EventIdT EVENT_COUNT = 0;

    MetaHeader(ThreadIdT THREAD_COUNT, EventIdT EVENT_COUNT, ResourceIdT VAR_COUNT, ResourceIdT LOCK_COUNT)
        : THREAD_COUNT(THREAD_COUNT), EVENT_COUNT(EVENT_COUNT), VAR_COUNT(VAR_COUNT), LOCK_COUNT(LOCK_COUNT){}
    
    MetaHeader(){}

    // Writes the fields one by one to trace_file
    // Do not use fwrite on the whole struct, it will also print padding
    void store(std::FILE* trace_file) const {
        std::fwrite(&THREAD_COUNT, sizeof(THREAD_COUNT), 1, trace_file);
        std::fwrite(&LOCK_COUNT, sizeof(LOCK_COUNT), 1, trace_file);
        std::fwrite(&VAR_COUNT, sizeof(VAR_COUNT), 1, trace_file);
        std::fwrite(&EVENT_COUNT, sizeof(EVENT_COUNT), 1, trace_file);
    }

    // Loads the fields one by one from trace_file
    void load(std::FILE* trace_file) {
        std::fread(&THREAD_COUNT, sizeof(THREAD_COUNT), 1, trace_file);
        std::fread(&LOCK_COUNT, sizeof(LOCK_COUNT), 1, trace_file);
        std::fread(&VAR_COUNT, sizeof(VAR_COUNT), 1, trace_file);
        std::fread(&EVENT_COUNT, sizeof(EVENT_COUNT), 1, trace_file);
    }

    // changes the byte order
    void byteswap(){
        THREAD_COUNT = std::byteswap(THREAD_COUNT);
        LOCK_COUNT = std::byteswap(LOCK_COUNT);
        VAR_COUNT = std::byteswap(VAR_COUNT);
        EVENT_COUNT = std::byteswap(EVENT_COUNT);
    }
};

struct MetaInfo{
    MetaHeader header;
    uint8_t LOCK_DEPTH = 8;
    std::vector<uint8_t> NOTIF_THREADS;

    void load_header(std::FILE* trace_file) {
        header.load(trace_file);
    }

    // Resized NOTIF_THREADS to THREAD_COUNT
    void resize_notif_threads(){
        NOTIF_THREADS.resize(header.THREAD_COUNT);
    }
};

inline MetaInfo meta_info;