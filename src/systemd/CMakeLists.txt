foreach (DAEMON ${daemons})
       if (${DAEMON} STREQUAL "korad_d" OR ${DAEMON} STREQUAL "vc820d" OR ${DAEMON} STREQUAL "rfio433d")
          SET(WANTED_BY "")
          SET(UNIT_EXTRALINES "StopWhenUnneeded=true")
       else ()
          SET(WANTED_BY "multi-user.target")
       endif (${DAEMON} STREQUAL "korad_d" OR ${DAEMON} STREQUAL "vc820d" OR ${DAEMON} STREQUAL "rfio433d")
       configure_file(slowcontrol.service.in ${DAEMON}.service)
       install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${DAEMON}.service DESTINATION /etc/systemd/system)
endforeach(DAEMON)

configure_file(50-korad-powersupply-udev.rules.in 50-korad-powersupply-udev.rules)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/50-korad-powersupply-udev.rules DESTINATION /etc/udev/rules.d)

configure_file(50-vc820-udev.rules.in 50-vc820-udev.rules)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/50-vc820-udev.rules DESTINATION /etc/udev/rules.d)

configure_file(50-rfio-udev.rules.in 50-rfio-udev.rules)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/50-rfio-udev.rules DESTINATION /etc/udev/rules.d)
