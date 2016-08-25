add_executable(onewired onewired.cpp)
target_link_libraries(onewired slowcontrol_static ${OptionParser_LIBRARIES})

add_executable(maassend maassend.cpp)
target_link_libraries(maassend slowcontrol_static ${OptionParser_LIBRARIES})
