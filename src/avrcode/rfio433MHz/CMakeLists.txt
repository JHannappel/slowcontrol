set(AVR_UPLOADTOOL_PORT /dev/ttyACM0)
set(AVR_MCU "atmega328p")
#     the type of AVR the application is built for
set(AVR_L_FUSE "0xBF")
#     the LOW fuse value for the MCU used
set(AVR_H_FUSE "0xD1")

include(generic-gcc-avr OPTIONAL RESULT_VARIABLE generic-gcc-avr-path)
if (NOT ${generic-gcc-avr-path} STREQUAL "NOTFOUND")

set(MCU_SPEED "16000000UL")
add_definitions("-DF_CPU=${MCU_SPEED}")

add_avr_executable(rfio433MHz rfio433MHz.cpp)

endif (NOT ${generic-gcc-avr-path} STREQUAL "NOTFOUND")
