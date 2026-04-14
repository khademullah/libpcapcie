cmake_minimum_required(VERSION 3.13)
project(pcapcie C)

set(CMAKE_C_STANDARD 11)

include_directories(include)

add_library(pcapcie
    src/pcapcie.c
    src/tlp.c
    src/filter.c
    src/backend_dummy.c
)

add_executable(sniff_and_send
    examples/sniff_and_send.c
)

target_link_libraries(sniff_and_send pcapcie)