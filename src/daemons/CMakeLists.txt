
include_directories("${CMAKE_SOURCE_DIR}/3rdParty/BMP280_driver")
add_executable(testdaemon testdaemon.cpp)
target_link_libraries(testdaemon slowcontrol_static ${OptionParser_LIBRARIES})


set(daemons maassend onewired bluetoothPresenced hih6131d gpiod raspicamd korad_d vc820d rfio433d ruleProcessord rpiBrightnessd bmp280d)
set(daemons ${daemons} PARENT_SCOPE)
foreach (DAEMON ${daemons})
	add_executable(${DAEMON} ${DAEMON}.cpp)
	target_link_libraries(${DAEMON} slowcontrol_static ${OptionParser_LIBRARIES})
        install(TARGETS ${DAEMON} DESTINATION bin)
endforeach(DAEMON)
