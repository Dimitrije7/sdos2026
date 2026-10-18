# Linux-side library and CLI. Built for the PC (host presets, with tests) and
# for the board's A72 cores (board-host presets).
find_package(Threads REQUIRED)

add_library(sdos_dsp STATIC
  libsdos_dsp/src/dsp.c
  libsdos_dsp/src/backend_linux.c
  libsdos_dsp/src/backend_mock.c
  libsdos_dsp/src/elf_check.c)
target_include_directories(sdos_dsp PUBLIC libsdos_dsp/include PRIVATE libsdos_dsp/src)
target_link_libraries(sdos_dsp PUBLIC sdos_protocol PRIVATE sdos_runtime sdos_cores)
sdos_target_warnings(sdos_dsp)

add_executable(dspctl dspctl/dspctl.c)
target_link_libraries(dspctl PRIVATE sdos_dsp sdos_runtime)
sdos_target_warnings(dspctl)

if(SDOS_PRODUCT STREQUAL "host")
  foreach(name dsp_mock dsp_linux)
    add_executable(test_${name} libsdos_dsp/tests/test_${name}.c)
    target_include_directories(test_${name} PRIVATE libsdos_dsp/src)
    target_link_libraries(test_${name} PRIVATE sdos_dsp sdos_runtime sdos_cores sdos_test_support Threads::Threads)
    sdos_target_warnings(test_${name})
    add_test(NAME ${name} COMMAND test_${name})
  endforeach()
  add_test(NAME dspctl_mock_cli
           COMMAND ${CMAKE_COMMAND} -DDSPCTL=$<TARGET_FILE:dspctl> -DWORK=${CMAKE_CURRENT_BINARY_DIR}/cli
                   -P ${CMAKE_CURRENT_SOURCE_DIR}/dspctl/tests/cli.cmake)
endif()
