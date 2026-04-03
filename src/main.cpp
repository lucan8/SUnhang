// Benchmark conclusions

// Generated

// Bensalem has one false positive due to thread timestamp inheritance being absent from the author's solution
// DBCP1 has one false positive due to thread timestamp inheritance being absent from the author's solution
// DBCP2 still kinda sus with 200 dlks(both theirs and my solution gave this result)
// Eclipse has one more cycles
// HASHMAP gives a higher number of abs dlk patterns than cycles, which is impossible
// IDENTITYHASHMAP gives a higher number of abs dlk patterns than cycles, which is impossible

// Original

// SUnhang3: JDBCMySQL-2: give 2 less dlks than SUnhang1, the same as SUnhang2, but with 2 less cycles
// SUnhang3 and SUnhang2 give the same dlk counts except for JDBC-MYSQL3 which failed for SUnhang2 because of an assert that got removed
// SUnahng4: DBCP1 +1 dlk, JDBC-MYSQL4 +1 dlk

// OBSERVATIONS/IMPROVEMENTS
//1. SOLVED (MENTION IT IN THE PAPER)
// Small mistake on their side, fork/join actually create fake thread entries in the maps
// For example fork(18) will create an entry 18 : id, and T18 will create another one which is wrong!
// They also seem to be counting acquires twice, this makes the benchmarks seem more impressive than they are

//3. 
// Another source of false positives appears because it can't track control flow

//4.

// Every lock will also have a read operation which is redundant

//5.
// Only looks at lock operations that use monitors(synchronized blocks)
// And ignores explicit locking like using java.util.concurrent.locks.Lock;

//IMPORTANT: Generic formatter for iterators
//TODO: ERR REPORT FILE FOR BAD TRACES
//TODO: Rename the comparison operators as they are actually biased toward the first argument
//TODO: How does this handle nested cycles?
//TODO: Add automatic formatting for your code
//TODO: Create namespace for util
//TODO: Think where to put your typedefs
//TODO: Rethink the graph situation
//TODO: Renames dependencies to nodes
//TODO: Think about the sentinel pattern
//TODO: Remove all asserts in release
//TODO: Use ranges instead of start and end iterators
//TODO: Template formater for vectors(YOU HAVE IT, USE IT)
//TODO: Pack the comparison operators of VectorClock together in one
//TODO: Timer function

// TODO: Bensalem asserts!
// Graph info for bensalem: 12 nodes, only 3 with outgoing neighbours, graph on the second to last page of your notebook

// ENCHANCEMENT:
// Don't stop at the first deadlock instance you find

// BIG QUESTION: Shouldn't the nodes(deps) be sorted based on when they appear in the trace?
// As keeping them in a mere map does not guarantee that ordering.
// ANSWER: NOP, dependencies can't really use the trace order, that's for events

//OPTIMIZATION:
// Instead of recomputing the SCCs everytime on the subgraph, take only the SCC from which the node was removed
// And run the algorithm only on that subgraph

// OPTIMIZATION:
// We could prune paths that can't be abstract deadlock patterns when we do cycle enumeration
// For example if we have the chain (t1, l2, {l1}) -> (t2, l3, {l2}) -> (t1, l1, {l3})
// We could stop looking at the path instantly as we are sure nothing will come out of it

// OPTIMIZATION:
// We could use vectors instead of maps for threads and resources as their ids are consecutive integers

#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <chrono>
#include <array>
#include <unordered_map>
#include <set>
#include <optional>
#include <future>
#include <cassert>
#include <filesystem>
using namespace std::chrono_literals;
namespace fs = std::filesystem;

#include "../include/trace_parser.hpp"
#include "../include/event_handler.hpp"
#include "../include/logger.hpp"
#include "../include/util.hpp"
#include "../include/test_vectorclock.hpp"
#include "../include/test_predictor.hpp"
#include "../include/scc_enumerator.hpp"
#include "../include/cycle_enumerator.hpp"
#include "../include/deadlock_checker.hpp"

int main(int argc, char *argv[]) {
    // const uint8_t exp_args = 4;
    // if (argc != exp_args){
    //     Logger::print(LogType::ERR, "Usage: ./SUnhang.exe [input_file_path] [out_summary_file_path] [extra_log_file_path]");
    //     return 1; 
    // }

    // const uint8_t exp_args = 4;
    // if (argc != exp_args){
    //     Logger::print(LogType::ERR, "Usage: ./SUnhang.exe [input_file_path] [out_summary_file_path] [bad_trace_rep_file]");
    //     return 1; 
    // }

    const uint8_t exp_args = 3;
    if (argc != exp_args){
        Logger::print(LogType::ERR, "Usage: ./SUnhang.exe [input_file_path] [out_summary_file_path]");
        return 1; 
    }

    auto start = std::chrono::system_clock::now();

    std::string trace_file_path = argv[1];
    std::string out_summ_path = argv[2];
    // std::string bad_trace_rep_path = argv[3];

    Logger::print(LogType::DBG, "Input path: {}", trace_file_path);
    Logger::print(LogType::DBG, "Out summary path: {}", out_summ_path);
    // Logger::print(LogType::DBG, "Err report path: {}", bad_trace_rep_path);

    std::ifstream trace_file(trace_file_path);
    if(!trace_file.good()) {
        Logger::print(LogType::ERR, "In file not found: {}", trace_file_path);
        return 1;
    }

    std::FILE* log_file(std::fopen(out_summ_path.c_str(), "w"));
    if (!log_file){
        Logger::print(LogType::ERR, "Log file not found: {}", out_summ_path);
        return 1;
    }

    // std::FILE* bad_trace_rep_file(std::fopen(bad_trace_rep_path.c_str(), "w"));
    // if (!bad_trace_rep_file){
    //     Logger::print(LogType::ERR, "Extra log file not found: {}", bad_trace_rep_path);
    //     return 1;
    // }

    // // Test stuff
    // TestVectorClock::test();
    // TestPredictor::test();

    TraceParser trace_parser(std::move(trace_file));
    EventHandler event_handler;

    // Trace parsing -> graph and critical section construction
    while (trace_parser.events_remaining()){
        auto event_opt = trace_parser.get_next_event();
        if (event_opt.has_value())
            event_handler.handle_event(event_opt.value());
    }
    event_handler.build_neigh_list();

    // Print summaries
    Logger::print(log_file, "----Trace info----");
    trace_parser.print_summary(log_file);
    event_handler.print_summary(log_file);
   
    // event_handler.print_abs_deps(extra_log_file);
    // Logger::print_dash_line(extra_log_file);
    // event_handler.print_neigh_list(extra_log_file);

    auto end = std::chrono::system_clock::now();
    auto millis_passed_parse_trace = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    CycleEnumerator cycle_enumerator(event_handler.graph_view);
    cycle_enumerator.enum_cycles();
    Logger::print(log_file, "num cycles: {}", cycle_enumerator.res_cycles.size());

    end = std::chrono::system_clock::now();
    auto millis_passed_cycle_enum = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    DeadlockChecker dlk_checker(event_handler.cs_hist);

    std::vector<int> abs_dlk_cycles_ind;
    abs_dlk_cycles_ind.reserve(32);
    for (int i = 0; i < cycle_enumerator.res_cycles.size(); ++i){
        bool is_abs_dlk = dlk_checker.is_abs_dlk_pattern(cycle_enumerator.res_cycles[i]);
        if (is_abs_dlk)
            abs_dlk_cycles_ind.push_back(i);
    }

    Logger::print(log_file, "num abstract: {}", abs_dlk_cycles_ind.size());
    Logger::print(log_file, "num concrete: -1\n"); // Just to match the format
    
    end = std::chrono::system_clock::now();
    auto millis_passed_abs_dlk_check = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    uint32_t real_dlk_count = 0;
    for (int i : abs_dlk_cycles_ind){
        auto dlk_info_opt = dlk_checker.get_sync_preserving_dlk(cycle_enumerator.res_cycles[i]);
        if (dlk_info_opt.has_value()){
            real_dlk_count += 1;
            auto dlk_info = dlk_info_opt.value();
            Logger::print(log_file, "Deadlock found on cycle: {}", dlk_info);
        }
    }

    end = std::chrono::system_clock::now();
    auto millis_passed_sync_pres_check = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    Logger::print(log_file, "\nnum deadlocks: {}", real_dlk_count);

    Logger::print(log_file, "Time for parsing and graph construction = {} milliseconds", millis_passed_parse_trace);
    Logger::print(log_file, "Time for cycle enumeration = {} milliseconds", millis_passed_cycle_enum - millis_passed_parse_trace);
    Logger::print(log_file, "Time for abs deadlock checks = {} milliseconds", millis_passed_abs_dlk_check - millis_passed_cycle_enum);
    Logger::print(log_file, "Time for sync pres check = {} milliseconds", millis_passed_sync_pres_check - millis_passed_abs_dlk_check);
    Logger::print(log_file, "{}", millis_passed_sync_pres_check);
    return 0;
}
