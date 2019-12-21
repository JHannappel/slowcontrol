# Check if cmake has the required version
CMAKE_MINIMUM_REQUIRED(VERSION 2.6.0 FATAL_ERROR)

# Set project name
PROJECT(Slowcontrol)

OPTION(INSTALL_DOCUMENTATION "Also install the HTML based API documentation (you first need to make the doc-target)" OFF)

SET(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/" "/usr/local/lib/cmake/OptionParser/" "/usr/local/lib/cmake/" )

include(CheckCXXCompilerFlag)

IF(NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
ENDIF(NOT CMAKE_BUILD_TYPE)
MESSAGE("Selected build type: ${CMAKE_BUILD_TYPE}")


CHECK_CXX_COMPILER_FLAG("-std=c++2a" COMPILER_SUPPORTS_CXX2a)

if(COMPILER_SUPPORTS_CXX2a)
  SET(CXX_STD_FLAG "-std=c++2a")
else()
  message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++2a support. Please use a different C++ compiler.")
endif()

find_package(OptionParser)
find_package(PostgreSQL)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CXX_STD_FLAG}")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wunused -Woverloaded-virtual -Wshadow -Wextra -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 -Wvla -Wsuggest-attribute=noreturn")
# Get latest tagged version.
# INCLUDE(GetLatestTag)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I ${OptionParser_INCLUDE_DIRS} -I ${PostgreSQL_INCLUDE_DIR}")

IF ( "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        MESSAGE("Compiler is CLANG, enabling additional parameters.")
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D__STRICT_ANSI__ -fdiagnostics-fixit-info -Qunused-arguments")
ENDIF ()

# Set variables for packaging. 
SET(PROJECT_DESCRIPTION "slowcontrol system written in C++2a")
SET(PROJECT_VERSION "${${CMAKE_PROJECT_NAME}_LATEST_TAG}")

# Write and install pkg-config file. 
#INCLUDE(WritePkgConfig)

# add a target to generate API documentation with Doxygen
find_package(Doxygen)

if(DOXYGEN_FOUND)
  set(doxyfile_in ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
  set(doxyfile ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

  configure_file(${doxyfile_in} ${doxyfile} @ONLY)

  add_custom_target(doc
    COMMAND ${DOXYGEN_EXECUTABLE} ${doxyfile}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating API documentation with Doxygen"
    VERBATIM)

  IF(INSTALL_DOCUMENTATION)
     install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html DESTINATION share/doc OPTIONAL)
  ENDIF()
ELSE()
  message(STATUS "Doxygen not found, not adding 'doc' target.")
endif()


# Finally, the main compilation... 
ADD_SUBDIRECTORY(src)
