cmake_minimum_required(VERSION 3.12)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
include($ENV{PICO_SDK_PATH}/tools/CMakeLists.txt)

project(GC9A01_test)
#project(tdc7200_test C CXX)
#set(CMAKE_C_STANDARD 11)
#set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(${PROJECT_NAME}  
    GC9A01/gc9a01.c
    gc9a01_test.c 
)

target_sources(${PROJECT_NAME} PRIVATE gc9a01_test.c 
    GC9A01/gc9a01.c
)

target_link_libraries(${PROJECT_NAME}
    pico_stdlib
    hardware_spi
)

pico_enable_stdio_usb(${PROJECT_NAME} 0)
pico_enable_stdio_uart(${PROJECT_NAME} 0)


# create map/bin/hex file etc.
pico_add_extra_outputs(${PROJECT_NAME})
