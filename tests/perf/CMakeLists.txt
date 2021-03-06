
# Copyright (c) 2013-2014, EPFL/Blue Brain Project
#                     Daniel Nachbaur <daniel.nachbaur@epfl.ch>
#                     Raphael Dumusc <raphael.dumusc@epfl.ch>
#
# Generates executables for the performance tests.

set(CTEST_CUSTOM_WARNING_EXCEPTION ${CTEST_CUSTOM_WARNING_EXCEPTION}
  # http://public.kitware.com/Bug/view.php?id=10179
  "ld: warning: duplicate dylib")
include(CTest)

enable_testing()
add_definitions(-DBOOST_TEST_DYN_LINK) # generates main() for unit tests
add_definitions(-DBOOST_PROGRAM_OPTIONS_DYN_LINK)

if(NOT WIN32) # tests want to be with DLLs on Windows - no rpath
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
endif()

set(TEST_LIBRARIES Deflect ${Boost_LIBRARIES} ${QT_QTGUI_LIBRARY})
set(PERF_TEST_SOURCES benchmarkStreamer.cpp streamTests.cpp)

# Create executables but do not add them to the tests target
foreach(FILE ${PERF_TEST_SOURCES})
  string(REGEX REPLACE ".cpp" "" NAME ${FILE})
  string(REGEX REPLACE "[./]" "_" NAME ${NAME})
  source_group(\\ FILES ${FILE})
  add_executable(${NAME} ${FILE})
  set_target_properties(${NAME} PROPERTIES FOLDER "Tests")
  target_link_libraries(${NAME} ${TEST_LIBRARIES})
endforeach()
