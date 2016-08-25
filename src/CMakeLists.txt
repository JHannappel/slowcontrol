include_directories("base/include")
set(CMAKE_EXE_LINKER_FLAGS "-lpq -lrt")
ADD_SUBDIRECTORY(base)
ADD_SUBDIRECTORY(daemons)
