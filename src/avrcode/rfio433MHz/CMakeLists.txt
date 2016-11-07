
set(AVR_MCU "atmega16a")
#     the type of AVR the application is built for
set(AVR_L_FUSE "BF")
#     the LOW fuse value for the MCU used
set(AVR_H_FUSE "D9")

set(MCU_SPEED "16000000UL")
add_definitions("-DF_CPU=${MCU_SPEED}")

add_avr_executable(rfio433MHz rfio433MHz.cpp)

