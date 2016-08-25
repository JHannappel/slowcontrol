add_library(slowcontrol_static STATIC slowcontrol.cpp measurement.cpp slowcontrolDaemon.cpp)
target_link_libraries(slowcontrol_static ${PostgreSQL_LIBRARIES} pthread)

