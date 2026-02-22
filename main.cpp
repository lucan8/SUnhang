// TODO: SEPARATOR SHOULD BE IN A COMMON FILE

#include <iostream>
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

using namespace std::chrono_literals;

#include "predictor.hpp"

std::unordered_map<std::string, events_t> std_event_map = {
    {"r", RD}, {"w", WR}, {"fork", FORK}, {"join", JOIN}, {"acq", LK}, {"rel", UK}
};


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
        if(result.event_type == FORK || result.event_type == JOIN) {
          result.target = get_id(std_thread_map, std_thread_counter, target);
        } 
        else if(result.event_type == LK || result.event_type == UK) {  
          result.target = get_id(std_lock_id_map, std_lock_id_counter , target);
        }      
	    else{
          result.target = get_id(std_var_map, std_var_counter, target);
	    }
      
      // Set src code location
      result.src_loc = std::stoi(current_result.at(2));
    } else { // Event not found
      result.event_type = NONE;
    }

    std_evt_counter += 1;

    return result;
}

// Splits str by sep
std::vector<std::string> split(const std::string& str, char sep){
    std::stringstream ss(str);
    std::vector<std::string> result;

    while(ss.good()) {
        std::string substring;
        getline(ss, substring, sep);
        result.push_back(substring);
    }

    return result;
}

void parse_trace(std::ifstream& file, std::string pred_name) {
    auto start_time_1 = std::chrono::steady_clock::now();
    Predictor predictor;

    std::string evt_str;
    int line_index = 0;
    const char sep = '|';
    const uint8_t exp_split_trace_size = 3;

    while(std::getline(file, evt_str)) {
        line_index++;
        std::vector<std::string> evt_str_split = split(evt_str, sep);

        if(evt_str_split.size() != exp_split_trace_size) {
            std::cout << "[WARN]: Bad file format on line " << line_index << ": " << evt_str << std::endl;
            continue;
        }
        
        // Convert the split std event with our mapping
        EventInfo event = from_std(evt_str_split);
        event.line = line_index;
        
        bool is_val_evt = predictor.handle_event(event);
        if (!is_val_evt)
            std::cout << "[WARN]: Invalid event on line " << line_index << ": " << evt_str << std::endl;

        event.print();
    }

    auto end_time_1 = std::chrono::steady_clock::now();
    size_t run_time_1 = std::chrono::duration_cast<std::chrono::milliseconds>(end_time_1 - start_time_1).count();

    std::cout << "[INFO]: Parsed and processed" << line_index << " lines in " << run_time_1 << "ms." << std::endl;
}

// Arg1 : trace file
// Arg2 : name of test
// Arg1 + Arg2 is used to build the name of the output files

int main(int argc, char *argv[]) {
    const uint8_t exp_args = 3;
    if (argc != exp_args){
        std::cout << "Usage: ./main.exe [input_file] [test_name]";
        return 1; 
    }

    std::string input_file = argv[1];
    std::string test_name = argv[2];
    std::string pred_name = input_file + test_name;

    std::ifstream file(input_file);
    if(!file.good()) {
        std::cout << "[ERROR]: File not found: " << input_file << std::endl;
        return 1;
    }

    reset_cnt_map();
    parse_trace(file,pred_name);
    return 0;
}
