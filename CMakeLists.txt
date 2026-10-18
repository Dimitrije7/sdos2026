# SDOS build entry point. Use the presets in CMakePresets.json through the
# command facade (`make configure|build|test PRESET=<name>`); see docs/commands.md.
cmake_minimum_required(VERSION 3.22)
project(sdos VERSION 0.1.0 LANGUAGES C)

include(cmake/SdosOptions.cmake)
include(cmake/SdosWarnings.cmake)
include(cmake/SdosCores.cmake)

if(SDOS_PRODUCT STREQUAL "host")
  enable_testing()
  add_library(sdos_test_support INTERFACE)
  target_include_directories(sdos_test_support INTERFACE tests/unit/include)
endif()

add_subdirectory(algorithms)
add_subdirectory(firmware)
if(NOT SDOS_PRODUCT STREQUAL "firmware")
  add_subdirectory(host)
endif()
