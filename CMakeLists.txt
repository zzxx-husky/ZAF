cmake_minimum_required(VERSION 3.5)
project(zaf VERSION 0.1.0 DESCRIPTION "ZMQ-based Actor Framework")

if (POLICY CMP0074)
  cmake_policy(SET CMP0074 NEW)
endif ()

if (NOT DEFINED ENABLE_TEST)
  set(ENABLE_TEST true)
endif ()
message("-- ENABLE_TEST: " ${ENABLE_TEST})
if (NOT DEFINED ENABLE_PHMAP)
  set(ENABLE_PHMAP true)
endif ()
message("-- ENABLE_PHMAP: " ${ENABLE_PHMAP})
set(BasicLibs pthread rt)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/")

if (DEFINED ENV{ZMQ_ROOT})
  set(ZMQ_ROOT_DIR $ENV{ZMQ_ROOT})
endif ()
find_package(ZMQ REQUIRED)
include_directories(${ZMQ_INCLUDE_DIR})
set(BasicLibs ${BasicLibs} ${ZMQ_LIBRARY})

# http://rpg.ifi.uzh.ch/docs/glog.html
if (DEFINED ENV{GLOG_ROOT})
  set(GLOG_ROOT $ENV{GLOG_ROOT})
endif ()
find_package(GLOG REQUIRED)
include_directories(${GLOG_INCLUDE_DIR})
set(BasicLibs ${BasicLibs} ${GLOG_LIBRARY})

#set(CMAKE_CXX_FLAGS_DEBUG "-g -fsanitize=address -fno-omit-frame-pointer")

if (${ENABLE_PHMAP})
  add_definitions(-DENABLE_PHMAP=1)

  if (DEFINED ENV{PHMAP_ROOT})
    set(PHMAP_ROOT $ENV{PHMAP_ROOT})
  endif ()
  find_package(PHMAP)
  include_directories(${PHMAP_ROOT_DIR})
endif ()

add_subdirectory(core)

if (${ENABLE_TEST})
  if (DEFINED ENV{GTEST_ROOT})
    set(GTEST_ROOT $ENV{GTEST_ROOT})
  endif ()

  find_package(GTest)
  include_directories(${GTEST_INCLUDE_DIR})
  set(TestLibs ${GTEST_LIBRARY})

  add_subdirectory(tests)
endif ()

add_subdirectory(examples)

# make install
include_directories(core)
set(ZAFSources ${CoreSources})
set(ZAFHeaders ${CoreHeaders})
add_library(zaf_static STATIC ${ZAFSources})
add_library(zaf_shared SHARED ${ZAFSources})
set_target_properties(zaf_static PROPERTIES
  OUTPUT_NAME zaf
  VERSION ${PROJECT_VERSION})
set_target_properties(zaf_shared PROPERTIES
  OUTPUT_NAME zaf
  VERSION ${PROJECT_VERSION})
include(GNUInstallDirs)
install(TARGETS zaf_static
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(TARGETS zaf_shared
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY core/zaf/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/zaf
  FILES_MATCHING
  PATTERN "*.hpp"
  PATTERN "*.tpp")
