#pragma once

#include "common_types.hpp"

struct MetaHeader{
    ThreadIdT THREAD_COUNT = 0;
    EventIdT EVENT_COUNT = 0;
    ResourceIdT VAR_COUNT = 0;
    ResourceIdT LOCK_COUNT = 0;

    MetaHeader(ThreadIdT THREAD_COUNT, EventIdT EVENT_COUNT, ResourceIdT VAR_COUNT, ResourceIdT LOCK_COUNT)
        : THREAD_COUNT(THREAD_COUNT), EVENT_COUNT(EVENT_COUNT), VAR_COUNT(VAR_COUNT), LOCK_COUNT(LOCK_COUNT){}
    
    MetaHeader(){}
};

struct MetaInfo{
    MetaHeader header;
    uint8_t LOCK_DEPTH = 8;
    std::vector<uint8_t> NOTIF_THREADS;

    void load_header(std::FILE* trace_file) {
        std::fread(&header, sizeof(header), 1, trace_file);
    }

    // Resized NOTIF_THREADS to THREAD_COUNT
    void resize_notif_threads(){
        NOTIF_THREADS.resize(header.THREAD_COUNT);
    }
};

inline MetaInfo meta_info;