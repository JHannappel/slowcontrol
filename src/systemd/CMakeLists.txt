#set(daemons maassend onewired bluetoothPresenced)

foreach (DAEMON ${daemons})
       configure_file(slowcontrol.service.in ${DAEMON}.service)
       install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${DAEMON}.service DESTINATION /etc/systemd/user)
endforeach(DAEMON)

install(FILES 50-korad-powersupply-udev.rules DESTINATION /etc/udev/rules.d)
