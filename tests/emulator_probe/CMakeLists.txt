cmake_minimum_required(VERSION 3.21)
project(RnetIsolatedEmulatorProbe LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTORCC ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(Threads REQUIRED)
if(NOT EMULATOR_SOURCE)
 message(FATAL_ERROR "EMULATOR_SOURCE must identify the existing emulator project")
endif()
file(GLOB NATIVE_SOURCES CONFIGURE_DEPENDS
 "${EMULATOR_SOURCE}/rollstuhl.emu/src/cyclic/*.cpp"
 "${EMULATOR_SOURCE}/rollstuhl.emu/src/session/*.cpp"
 "${EMULATOR_SOURCE}/rollstuhl.emu/src/statemachine/*.cpp")
add_executable(rnet_emulator_gui_probe gui_probe.cpp ${NATIVE_SOURCES}
 "${EMULATOR_SOURCE}/rollstuhl.emu/resources.qrc"
 "${EMULATOR_SOURCE}/rollstuhl.emu/RNetCanIOWrap.cpp")
target_compile_definitions(rnet_emulator_gui_probe PRIVATE EMULATOR_MAIN_SOURCE="${EMULATOR_SOURCE}/rollstuhl.emu/src/main.cpp")
target_include_directories(rnet_emulator_gui_probe PRIVATE "${EMULATOR_SOURCE}/rollstuhl.emu/src" "${EMULATOR_SOURCE}/common")
target_link_libraries(rnet_emulator_gui_probe PRIVATE Qt6::Widgets Threads::Threads)
foreach(name IN ITEMS write read send recv sendto recvfrom sendmsg recvmsg writev readv)
 target_link_options(rnet_emulator_gui_probe PRIVATE "-Wl,--wrap=${name}")
endforeach()
add_executable(rnet_test_can_proxy "${EMULATOR_SOURCE}/tools/rnet-can-proxy/main.cpp")
