# SDOS build entry point. Use the presets in CMakePresets.json through the
# command facade (`make configure|build|test PRESET=<name>`); see docs/commands.md.
cmake_minimum_required(VERSION 3.22)
project(sdos VERSION 0.1.0 LANGUAGES C)

include(cmake/SdosOptions.cmake)
include(cmake/SdosWarnings.cmake)

if(SDOS_PRODUCT STREQUAL "host")
  enable_testing()
endif()

add_subdirectory(algorithms)
