add_executable(testdaemon testdaemon.cpp)
target_link_libraries(testdaemon slowcontrol_static ${OptionParser_LIBRARIES})

add_executable(onewired onewired.cpp)
target_link_libraries(onewired slowcontrol_static ${OptionParser_LIBRARIES})
install(TARGETS onewired DESTINATION bin)

add_executable(maassend maassend.cpp)
target_link_libraries(maassend slowcontrol_static ${OptionParser_LIBRARIES})
install(TARGETS maassend DESTINATION bin)

add_executable(bluetoothPresenced bluetoothPresenced.cpp)
target_link_libraries(bluetoothPresenced slowcontrol_static ${OptionParser_LIBRARIES})
install(TARGETS bluetoothPresenced DESTINATION bin)
