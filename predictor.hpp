#pragma once

#include <iostream>
#include "predictor_types.hpp"
#include "logger.hpp"

struct Predictor{
  
  // Calls handler associated with evt.event_type
  // Return true if event if valid, false otherwise
  bool handle_event(const EventInfo& evt){
      switch (evt.event_type){
        case EventsT::LK:
          acquire_event(evt);
          break;
        case EventsT::UK:
          release_event(evt);
          break;
        case EventsT::RD:
          read_event(evt); 
          break;
        case EventsT::WR:
          write_event(evt);
          break;
        case EventsT::FORK:
          fork_event(evt);
          break;
        case EventsT::JOIN:
          join_event(evt);
          break;
        default:
            return false;
    }
    return true;
  }

  void read_event(const EventInfo& trace_line) {
    Logger::print(LogType::DBG, "Read event");
  }

  void write_event(const EventInfo& trace_line) {
    Logger::print(LogType::DBG, "Write event");
  }

  void acquire_event(const EventInfo& trace_line) {
    Logger::print(LogType::DBG, "Acquire event");        
  }

  void release_event(const EventInfo& trace_line) {
    Logger::print(LogType::DBG, "Release event");
  }
  
  void fork_event(const EventInfo& trace_line) {
    Logger::print(LogType::DBG, "Fork event");
  }

  void join_event(const EventInfo& trace_line) {
    Logger::print(LogType::DBG, "Join event");
  }
};