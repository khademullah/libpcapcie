cmake_minimum_required(VERSION 3.13)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

add_executable(pcieshark_gui
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
)

set_target_properties(pcieshark_gui PROPERTIES OUTPUT_NAME "pcieshark")

target_include_directories(pcieshark_gui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(pcieshark_gui PRIVATE pcapcie Qt6::Widgets)
add_executable(pcie_trace_viewer ALIAS pcieshark_gui)
