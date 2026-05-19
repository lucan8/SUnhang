set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
cmake_minimum_required(VERSION 4.0)

project(SUnhang)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../bin")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
# set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--verbose")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


set(SOURCE_FILES
    src/logger.cpp

    src/test_predictor.cpp
    src/event_handler.cpp
    src/trace_parser.cpp
    
    src/test_vectorclock.cpp
    src/vectorclock.cpp

    src/ord_dep_graph.cpp
    src/scc_enumerator.cpp
    src/cycle_enumerator.cpp
    src/deadlock_checker.cpp
)

add_executable(SUnhang src/main.cpp ${SOURCE_FILES})
# add_executable(conv_trace src/conv_trace.cpp ${SOURCE_FILES})

# For std print
target_link_libraries(SUnhang PRIVATE -lstdc++exp)
# target_link_libraries(conv_trace PRIVATE -lstdc++exp)