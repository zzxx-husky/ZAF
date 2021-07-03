cmake_minimum_required(VERSION 3.5)
project(zaf VERSION 0.1.0 DESCRIPTION "ZMQ-based Actor Framework")

set(ENABLE_TEST true)
set(ENABLE_PHMAP true)
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
add_library(zaf STATIC ${ZAFSources})
set_target_properties(zaf PROPERTIES
  VERSION ${PROJECT_VERSION})
include(GNUInstallDirs)
install(TARGETS zaf
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY core/zaf/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/zaf
  FILES_MATCHING
  PATTERN "*.hpp"
  PATTERN "*.tpp")
