# Protocol and runtime shared by Linux and all DSP firmware (docs/protocol.md).
add_library(sdos_protocol STATIC protocol/src/protocol.c)
target_include_directories(sdos_protocol PUBLIC protocol/include)
sdos_target_warnings(sdos_protocol)

add_library(sdos_runtime STATIC runtime/src/runtime.c)
target_include_directories(sdos_runtime PUBLIC runtime/include)
target_link_libraries(sdos_runtime PUBLIC sdos_protocol PRIVATE sdos_fir_q15)
sdos_target_warnings(sdos_runtime)

if(SDOS_PRODUCT STREQUAL "host")
  foreach(name protocol runtime)
    add_executable(test_${name} ${name}/tests/test_${name}.c)
    target_link_libraries(test_${name} PRIVATE sdos_runtime sdos_test_support)
    sdos_target_warnings(test_${name})
    add_test(NAME ${name} COMMAND test_${name})
  endforeach()
endif()
