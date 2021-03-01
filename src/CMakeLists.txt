cmake_minimum_required(VERSION 3.17)
project(dc_server C)

set(CMAKE_C_STANDARD 11)
include_directories(/usr/local/include)
link_directories(/usr/local/lib)

add_library(dc_server SHARED server.c)
target_compile_options(dc_server PRIVATE -Wpedantic -Wall -Wextra -g)

find_library(LIBDC_FSM dc_fsm)
if(LIBDC_FSM)
    target_link_libraries(dc_server ${LIBDC_FSM})
endif()

install(TARGETS dc_server DESTINATION lib)
install(FILES server.h server_config.h DESTINATION include/dc_server)
