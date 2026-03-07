//TODO: Add automatic formatting for your code
//TODO: Either use format everywhere or show, not both(USE PRINT!)
//TODO: Functions for stats and tests for graph construction (dependencies, locks, variables, events)
//TODO: Create namespace for util
//TODO: Think where to put your typedefs
//TODO: Rethink the graph situation
//TODO: Renames dependencies to nodes
//TODO: Think about the sentinel pattern
//TODO: Remove all asserts in release
//TODO: Look into using ranges instead of start and end iterators
//TODO: Template formater for vectors
//TODO: Make AbsDep point to an iterator of a VC instead of the actual VC
//TODO: Pack the comparison operators of VectorClock together in one

// BIG QUESTION: Shouldn't the nodes(deps) be sorted based on when they appear in the trace?
// As keeping them in a mere map does not guarantee that ordering.
// ANSWER: NOP, dependencies can't really use the trace order, that's for events

// TEST IDEEA: Run this on all the benchmarks, save information to a csv file and then check the csvs
// against the author's
 
//SIMPLE OPTIMIZATION: IGNORE FIRST LEVEL LOCK ACQUISITIONS!

//OPTIMIZATION:
// Instead of recomputing the SCCs everytime on the subgraph, take only the SCC from which the node was removed
// And run the algorithm only on that subgraph

// OPTIMIZATION:
// We could prune paths that can't be abstract deadlock patterns when we do cycle enumeration
// For example if we have the chain (t1, l2, {l1}) -> (t2, l3, {l2}) -> (t1, l1, {l3})
// We could stop looking at the path instantly as we are sure nothing will come out of it

// TODO: Bensalem asserts!
// Graph info for bensalem: 12 nodes, only 3 with outgoing neighbours, graph on the second to last page of your notebook

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
using namespace std::chrono_literals;

#include "../include/predictor.hpp"
#include "../include/logger.hpp"
#include "../include/util.hpp"
#include "../include/test_vectorclock.hpp"
#include "../include/test_predictor.hpp"
#include "../include/scc_enumerator.hpp"
#include "../include/cycle_enumerator.hpp"
#include "../include/deadlock_checker.hpp"

// Maps for converting from std format
size_t std_lock_id_counter = 0;
std::unordered_map<std::string, int> std_lock_id_map = {};
size_t std_thread_counter = 0;
std::unordered_map<std::string, int> std_thread_map = {};
size_t std_var_counter = 0;
std::unordered_map<std::string, int> std_var_map = {};

size_t std_evt_counter = 0;

void reset_cnt_map() {
 std_lock_id_counter = 0;
 std_thread_counter = 0;
 std_var_counter = 0;
 std_evt_counter = 0;

 std_lock_id_map.clear();
 std_thread_map.clear();
 std_var_map.clear();

}

// Returns the corresponding id for std_id from std_id_map
// Updates the map and counter if not found
int get_id(std::unordered_map<std::string, int>& std_id_map, size_t& id_counter, const std::string& std_id){
  auto map_entry = std_id_map.find(std_id);
  int result_id;

  if(map_entry == std_id_map.end()) {
      std_id_map[std_id] = id_counter;
      result_id = id_counter;
      id_counter += 1;
    } else {
      result_id = map_entry->second;
    }

    return result_id;
}

// Map the std formated result to our custom result using the std_* maps
// std format ex: T1|acq(l1)|25 might turn into 1, 1, 1 uisng the std_* maps
EventInfo from_std(const std::vector<std::string>& current_result) {
    EventInfo result = {};

    // Set thread id
    result.thread_id = get_id(std_thread_map, std_thread_counter, current_result.at(0));

    // Set event type
    auto event_len = current_result.at(1).find("(");
    auto event_type = std_event_map.find(current_result.at(1).substr(0, event_len));

    // Event found
    if(event_type != std_event_map.end()) {
        result.event_type = event_type->second;
        // The event target(the variable on which the event is happening)
        auto target = current_result.at(1).substr(event_len + 1, current_result.at(1).length() - event_len - 2);

        // Update maps depending on operation
        if(result.event_type == EventsT::FORK || result.event_type == EventsT::JOIN) {
          result.target = get_id(std_thread_map, std_thread_counter, target);
        } 
        else if(result.event_type == EventsT::LK || result.event_type == EventsT::UK) {  
          result.target = get_id(std_lock_id_map, std_lock_id_counter , target);
        }      
	    else{
          result.target = get_id(std_var_map, std_var_counter, target);
	    }
      
      // Set src code location
      result.src_loc = std::stoi(current_result.at(2));
    } else { // Event not found
      result.event_type = EventsT::NONE;
    }

    std_evt_counter += 1;

    return result;
}

void parse_trace(Predictor& predictor, std::ifstream& file, const std::string& pred_name) {
    auto start_time_1 = std::chrono::steady_clock::now();

    std::string evt_str;
    int line_index = 0;

    while(std::getline(file, evt_str)) {
        line_index++;
        std::vector<std::string> evt_str_split = split(evt_str, trace_sep);

        if(evt_str_split.size() != exp_split_trace_size) {
            Logger::print(LogType::WARN, "Bad file format on line {}: {}", line_index, evt_str);
            continue;
        }
        
        // Convert the split std event with our mapping
        EventInfo event = from_std(evt_str_split);
        event.line = line_index;
        
        bool is_val_evt = predictor.handle_event(event);
        if (!is_val_evt)
            Logger::print(LogType::WARN, "Invalid event on line {}: {}", line_index, evt_str);

        Logger::print(LogType::DBG, "{}", event);
    }

    auto end_time_1 = std::chrono::steady_clock::now();
    size_t run_time_1 = std::chrono::duration_cast<std::chrono::milliseconds>(end_time_1 - start_time_1).count();

    Logger::print(LogType::INFO, "Parsed and processed {} lines in {}ums.", line_index, run_time_1);
}

// Arg1 : trace file
// Arg2 : name of test
// Arg1 + Arg2 is used to build the name of the output files

int main(int argc, char *argv[]) {
    const uint8_t exp_args = 3;
    if (argc != exp_args){
        Logger::print(LogType::ERR, "Usage: ./main.exe [input_file] [test_name]");
        return 1; 
    }

    std::string input_file = argv[1];
    std::string test_name = argv[2];
    std::string pred_name = input_file + test_name;

    // Logger::print(LogType::DBG, "Constructed pred_name: {}", pred_name);
    std::ifstream file(input_file);
    if(!file.good()) {
        Logger::print(LogType::ERR, "File not found: {}", input_file);
        return 1;
    }

    // Test stuff
    TestVectorClock::test();
    TestPredictor::test();

    reset_cnt_map();
    Predictor predictor;
    parse_trace(predictor, file, pred_name);

    predictor.build_neigh_list();
    
    predictor.print_neigh_list();
    predictor.print_lock_deps_map();
    predictor.print_abs_deps();

    SCCEnumerator scc_enumerator(predictor.graph_view);
    scc_enumerator.get_min_strong_conn_comp();
    scc_enumerator.print_info();

    CycleEnumerator cycle_enumerator(predictor.graph_view);
    cycle_enumerator.enum_cycles();
    cycle_enumerator.print_info();

    DeadlockChecker dlk_checker;
    Logger::print(LogType::INFO, "DEADLOCK CHECKER INFORMATION");
    Logger::print_dash_line();

    for (int i = 0; i < cycle_enumerator.res_cycles.size(); ++i){
        Logger::print(LogType::DBG, "CYCLE {}: {}\n", i, dlk_checker.is_abs_dlk_pattern(cycle_enumerator.res_cycles[i]));
    }

    Logger::print_dash_line();

    return 0;
}
