cmake_minimum_required(VERSION 3.13)
project(pcapcie C)

set(CMAKE_C_STANDARD 11)

include_directories(include)

add_library(pcapcie
    src/pcapcie.c
    src/tlp.c
    src/filter.c
    src/backend_dummy.c
    src/backend_fpga.c
    src/backend_armds.c
    src/backend_xgig.c
    src/backend_pci.c
)

add_executable(sniff_and_send
    examples/sniff_and_send.c
)

add_executable(simple_tlp_test
    examples/simple_tlp_test.c
)

target_link_libraries(sniff_and_send pcapcie)
target_link_libraries(simple_tlp_test pcapcie)
