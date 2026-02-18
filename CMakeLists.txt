cmake_minimum_required(VERSION 3.16)
project(traffic_sim C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Wpedantic)

# Core library
#
# Will add new .c files here as modules are implemented (intersection.c,
# simulation.c, hal_desktop.c, main.c).
set(CORE_SOURCES
    src/road.c
    src/traffic_light.c
    src/controller.c
    src/intersection.c
)

add_library(traffic_sim_core STATIC ${CORE_SOURCES})
target_include_directories(traffic_sim_core PUBLIC include)

# Main executable (when main.c is implemented)
#
# add_executable(traffic_sim src/main.c)
# target_link_libraries(traffic_sim PRIVATE traffic_sim_core)

# Tests
enable_testing()

set(TEST_NAMES road traffic_light controller intersection)

foreach(name ${TEST_NAMES})
    add_executable(test_${name} tests/test_${name}.c)
    target_link_libraries(test_${name} PRIVATE traffic_sim_core)
    # tests/ is on the include path for test_helpers.h
    target_include_directories(test_${name} PRIVATE tests)
    add_test(NAME test_${name} COMMAND test_${name})
endforeach()
