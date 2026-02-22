#include <iostream>
#include "predictor_types.hpp"

struct Predictor{
  
  // Calls handler associated with evt.event_type
  // Return true if event if valid, false otherwise
  bool handle_event(const EventInfo& evt){
      switch (evt.event_type){
        case LK:
          acquire_event(evt);
          break;
        case UK:
          release_event(evt);
          break;
        case RD:
          read_event(evt); 
          break;
        case WR:
          write_event(evt);
          break;
        case FORK:
          fork_event(evt);
          break;
        case JOIN:
          join_event(evt);
          break;
        default:
            return false;
    }
    return true;
  }

  void read_event(const EventInfo& trace_line) {
    std::cout << "Read event\n";
  }

  void write_event(const EventInfo& trace_line) {
    std::cout << "Write event\n";
  }

  void acquire_event(const EventInfo& trace_line) {
    std::cout << "Acq event\n";         
  }

  void release_event(const EventInfo& trace_line) {
    std::cout << "Rel event\n";
  }
  
  void fork_event(const EventInfo& trace_line) {
    std::cout << "Fork event\n";
  }

  void join_event(const EventInfo& trace_line) {
    std::cout << "Join event\n";
  }
};