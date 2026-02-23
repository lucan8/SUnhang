#include "../include/predictor.hpp"
#include "../include/logger.hpp"

bool Predictor::handle_event(const EventInfo& evt){
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

void Predictor::read_event(const EventInfo& trace_line) {
    Logger::print(LogType::DBG, "Read event");
}

void Predictor::write_event(const EventInfo& trace_line) {
    Logger::print(LogType::DBG, "Write event");
}

void Predictor::acquire_event(const EventInfo& trace_line) {
    Logger::print(LogType::DBG, "Acquire event");        
}

void Predictor::release_event(const EventInfo& trace_line) {
    Logger::print(LogType::DBG, "Release event");
}

void Predictor::fork_event(const EventInfo& trace_line) {
    Logger::print(LogType::DBG, "Fork event");
}

void Predictor::join_event(const EventInfo& trace_line) {
    Logger::print(LogType::DBG, "Join event");
}