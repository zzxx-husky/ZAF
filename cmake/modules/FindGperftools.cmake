#.rst:
# FindGperftools.cmake
# -------------
#
# Find a Google gperftools installation.
#
# This module finds if Google gperftools is installed and selects a default
# configuration to use.
#
#   find_package(Gperftools COMPONENTS ...)
#
# Valid components are:
#
#   PROFILER
#
# The following variables control which libraries are found::
#
#   Gperftools_USE_STATIC_LIBS  - Set to ON to force use of static libraries.
#
# The following are set after the configuration is done:
#
# ::
#
#   Gperftools_FOUND            - Set to TRUE if gperftools was found
#   Gperftools_INCLUDE_DIRS     - Include directories
#   Gperftools_LIBRARIES        - Path to the gperftools libraries
#   Gperftools_LIBRARY_DIRS     - Compile time link directories
#   Gperftools_<component>      - Path to specified component
#
#
# Sample usage:
#
# ::
#
#   find_package(Gperftools)
#   if(Gperftools_FOUND)
#     target_link_libraries(<YourTarget> ${Gperftools_LIBRARIES})
#   endif()

if (Gperftools_USE_STATIC_LIBS)
  set(_CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})
  set(CMAKE_FIND_LIBRARY_SUFFIXES .lib .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
endif ()

macro(_find_library libvar libname)
  find_library(${libvar}
    NAMES ${libname}
    PATH ${GPERF_ROOT}/lib
    )
  if (NOT ${libvar}_NOTFOUND)
    set(${libvar}_FOUND TRUE)
  endif ()
endmacro()

_find_library(Gperftools_PROFILER profiler)
_find_library(Gperftools_TCMALLOC tcmalloc)
_find_library(Gperftools_TCMALLOC_AND_PROFILER tcmalloc_and_profiler)

find_path(Gperftools_INCLUDE_DIR
  NAMES gperftools/heap-profiler.h
  PATHS ${GPERF_ROOT}/include
  NO_DEFAULT_PATH
  )

find_library(Gperftools_LIBRARY_DIR
  NAMES profiler
  PATHS ${GPERF_ROOT}/lib
  NO_DEFAULT_PATH
  )

# Set standard CMake FindPackage variables if found.
set(Gperftools_INCLUDE_DIRS ${Gperftools_INCLUDE_DIR})
set(Gperftools_LIBRARY_DIRS ${Gperftools_LIBRARY_DIR})

if (Gperftools_USE_STATIC_LIBS)
  set(CMAKE_FIND_LIBRARY_SUFFIXES ${_CMAKE_FIND_LIBRARY_SUFFIXES})
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  Gperftools
  FOUND_VAR Gperftools_FOUND
  REQUIRED_VARS Gperftools_INCLUDE_DIR
  HANDLE_COMPONENTS
)

if (Gperftools_FOUND)
  message(STATUS "Found valid Gperftools version:")
  message(STATUS "  Gperftools root dir: ${GPERF_ROOT}")
  message(STATUS "  Gperftools include dir: ${Gperftools_INCLUDE_DIR}")
  message(STATUS "  Gperftools libraries: ${Gperftools_LIBRARY_DIR}")
endif ()
