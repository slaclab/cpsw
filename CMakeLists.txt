cmake_minimum_required(VERSION 3.18)

project(cpsw CXX C)

####################################################
# Configuration Options
####################################################
set(WITH_SHARED_LIBRARIES ON CACHE BOOL "")
set(WITH_STATIC_LIBRARIES ON CACHE BOOL "")
set(WITH_PYCPSW "BOOST" CACHE STRING "")
set(WITH_BOOST ON CACHE BOOL "")
set(WITH_CXX11 ON CACHE BOOL "")
set(WITH_TIRPC OFF CACHE BOOL "")
####################################################

include(FetchContent)
include(CheckIncludeFile)
include(GNUInstallDirs)

# Always build with -fPIC
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(PkgConfig REQUIRED)

####################################################
# Boost, Python and Cython
####################################################

set(NO_PYTHON 0)
if ("${WITH_PYCPSW}" STREQUAL "BOOST")
	find_package(
    	Boost REQUIRED COMPONENTS Python
	)
	message(STATUS "Using BOOST for Python bindings")
elseif ("${WITH_PYCPSW}" STREQUAL "CYTHON")
	include(cmake/FindCython.cmake)
	find_package(
		cython REQUIRED
	)
	message(STATUS "Using Cython for Python bindings")
elseif ("${WITH_PYCPSW}" STREQUAL "NO")
	set(NO_PYTHON 1)
	message(STATUS "CPSW Python bindings are disabled")
else()
	message(FATAL_ERROR "Invalid choice, WITH_PYCPSW must be BOOST, CYTHON or NO")
endif()

if (NOT NO_PYTHON)
	find_package(
		Python REQUIRED COMPONENTS Development
	)
endif()

####################################################
# tirpc
####################################################

# Check for RPC library
check_include_file(rpc/rpc.h HAVE_RPC)

if (NOT HAVE_RPC)
	message(STATUS "glibc's RPC library not detected; using libtirpc instead")
	set(WITH_TIRPC ON FORCE)
	pkg_check_modules(TIRPC REQUIRED libtirpc)
endif()

if (WITH_CXX11)
	set(CMAKE_CXX_STANDARD 11)
endif()

####################################################
# yaml-cpp
####################################################

FetchContent_Declare(
	yaml-cpp
	GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
	GIT_TAG 0.8.0
)
FetchContent_MakeAvailable(yaml-cpp)

####################################################
# Misc
####################################################

# Generate a version string
execute_process(
	COMMAND bash -c "echo -n \\\"`git describe --always --dirty`\\\""
	OUTPUT_VARIABLE CPSW_GIT_VERSION_STRING
)

message(STATUS "CPSW_GIT_VERSION_STRING=${CPSW_GIT_VERSION_STRING}")

string(REGEX MATCH "\"?R([0-9]+)\.([0-9]+)\.([0-9]+)-([0-9]+)" CPSW_API_VERSION "${CPSW_GIT_VERSION_STRING}")

set(CPSW_API_VERSION "${CMAKE_MATCH_1}")
set(CPSW_MAJOR_VERSION "${CMAKE_MATCH_1}")
set(CPSW_MINOR_VERSION "${CMAKE_MATCH_2}")
set(CPSW_REVISION "${CMAKE_MATCH_3}")
set(CPSW_DELTA_COMMIT "${CMAKE_MATCH_4}")

message(STATUS "CPSW_API_VERSION=${CPSW_API_VERSION}")
message(STATUS "CPSW_MAJOR_VERSION=${CPSW_MAJOR_VERSION}")
message(STATUS "CPSW_MINOR_VERSION=${CPSW_MINOR_VERSION}")
message(STATUS "CPSW_REVISION=${CPSW_REVISION}")
message(STATUS "CPSW_DELTA_COMMIT=${CPSW_DELTA_COMMIT}")


add_subdirectory(src)
