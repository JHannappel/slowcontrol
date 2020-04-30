add_library(slowcontrol_static STATIC slowcontrol.cpp measurement.cpp slowcontrolDaemon.cpp states.cpp gpio.cpp communications.cpp trigger.cpp configValue.cpp pgsqlWrapper.cpp)
target_link_libraries(slowcontrol_static ${PostgreSQL_LIBRARIES} pthread atomic)

