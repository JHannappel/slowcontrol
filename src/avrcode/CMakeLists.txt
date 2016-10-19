
set(AVR_MCU "atmega16a")
#     the type of AVR the application is built for
set(AVR_L_FUSE "BF")
#     the LOW fuse value for the MCU used
set(AVR_H_FUSE "D9")

set(MCU_SPEED "16000000UL")

set(AVR_UPLOADTOOL_PORT /dev/ttyACM0)
include(generic-gcc-avr OPTIONAL RESULT_VARIABLE generic-gcc-avr-path)

if (NOT ${generic-gcc-avr-path} STREQUAL "NOTFOUND")

set(CMAKE_C_FLAGS "-O3")
set(CMAKE_CXX_FLAGS "-O3 -std=c++11")
set(CMAKE_EXE_LINKER_FLAGS "")
set(CMAKE_EXE_LINKER_FLAGS_RelWithDebInfo "")
SET(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS)
SET(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS)

add_definitions("-DF_CPU=${MCU_SPEED}")
add_definitions("-fpack-struct")
add_definitions("-fshort-enums")
add_definitions("-Wall")
add_definitions("-Werror")
add_definitions("-pedantic")
add_definitions("-pedantic-errors")
add_definitions("-funsigned-char")
add_definitions("-funsigned-bitfields")
add_definitions("-ffunction-sections")
add_definitions("-c")

add_avr_executable(avrtest avrtest.cpp)
endif (NOT ${generic-gcc-avr-path} STREQUAL "NOTFOUND")
