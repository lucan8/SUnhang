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


set(SUNHANG_SRC
    src/logger.cpp

    src/event_handler.cpp
    src/bin_trace_parser.cpp
    
    src/vectorclock.cpp

    src/ord_dep_graph.cpp
    src/scc_enumerator.cpp
    src/cycle_enumerator.cpp
    src/deadlock_checker.cpp
)

set(CONV_TR_SRC
    src/logger.cpp
    src/std_trace_parser.cpp
)

add_executable(SUnhang src/main.cpp ${SUNHANG_SRC})
add_executable(conv_trace src/conv_trace.cpp ${CONV_TR_SRC})

# For std print
target_link_libraries(SUnhang PRIVATE -lstdc++exp)
target_link_libraries(conv_trace PRIVATE -lstdc++exp)