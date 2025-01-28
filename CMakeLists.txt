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

find_package(PkgConfig REQUIRED)

if ("${WITH_PYCPSW}" STREQUAL "BOOST")
	find_package(
    	boost COMPONENTS Python
	)
elseif ("${WITH_PYCPSW}" STREQUAL "CYTHON")
	include(cmake/FindCython.cmake)
	find_package(
		cython
	)
endif()

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


FetchContent_Declare(
	yaml-cpp
	GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
	GIT_TAG 0.8.0
)
FetchContent_MakeAvailable(yaml-cpp)


add_subdirectory(src)
