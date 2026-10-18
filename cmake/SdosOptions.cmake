# Build dimensions shared by every preset. A preset selects one product and,
# for firmware, one DSP core and one runtime; invalid combinations stop here.

set(SDOS_PRODUCT "host" CACHE STRING "host, board-host, or firmware")
set_property(CACHE SDOS_PRODUCT PROPERTY STRINGS host board-host firmware)
set(SDOS_CORE "none" CACHE STRING "DSP core for firmware: c66x_0, c66x_1, or c71x_0")
set_property(CACHE SDOS_CORE PROPERTY STRINGS none c66x_0 c66x_1 c71x_0)
set(SDOS_RTOS "none" CACHE STRING "Firmware runtime: freertos or baremetal")
set_property(CACHE SDOS_RTOS PROPERTY STRINGS none freertos baremetal)
option(SDOS_WERROR "Treat compiler warnings as errors" OFF)

if(NOT SDOS_PRODUCT MATCHES "^(host|board-host|firmware)$")
  message(FATAL_ERROR "SDOS_PRODUCT=${SDOS_PRODUCT} is not one of host, board-host, firmware")
endif()

if(SDOS_PRODUCT STREQUAL "firmware")
  if(NOT SDOS_CORE MATCHES "^(c66x_0|c66x_1|c71x_0)$")
    message(FATAL_ERROR "firmware needs SDOS_CORE c66x_0, c66x_1, or c71x_0; got ${SDOS_CORE}")
  endif()
  if(NOT SDOS_RTOS MATCHES "^(freertos|baremetal)$")
    message(FATAL_ERROR "firmware needs SDOS_RTOS freertos or baremetal; got ${SDOS_RTOS}")
  endif()
  if(SDOS_CORE MATCHES "^c66x" AND NOT SDOS_TI_FAMILY STREQUAL "c6000")
    message(FATAL_ERROR "${SDOS_CORE} needs the C6000 toolchain file")
  endif()
  if(SDOS_CORE STREQUAL "c71x_0" AND NOT SDOS_TI_FAMILY STREQUAL "c7000")
    message(FATAL_ERROR "c71x_0 needs the C7000 toolchain file")
  endif()
elseif(NOT SDOS_CORE STREQUAL "none" OR NOT SDOS_RTOS STREQUAL "none")
  message(FATAL_ERROR "SDOS_CORE and SDOS_RTOS apply only to SDOS_PRODUCT=firmware")
endif()

if(SDOS_PRODUCT STREQUAL "firmware")
  # A TI release such as 5.0.0.LTS reports its numeric part as the compiler version.
  string(REGEX MATCH "^[0-9]+\\.[0-9]+\\.[0-9]+" _expected "${SDOS_TI_EXPECTED_VERSION}")
  if(NOT CMAKE_C_COMPILER_VERSION VERSION_EQUAL _expected)
    message(FATAL_ERROR "TI compiler ${CMAKE_C_COMPILER} is version ${CMAKE_C_COMPILER_VERSION}; "
                        "the manifest pins ${SDOS_TI_EXPECTED_VERSION}. Run `make bootstrap`.")
  endif()
endif()

# Record what built this tree, so every artifact traces to manifest pins.
file(WRITE "${CMAKE_BINARY_DIR}/sdos-build-info.json"
"{
  \"product\": \"${SDOS_PRODUCT}\",
  \"core\": \"${SDOS_CORE}\",
  \"rtos\": \"${SDOS_RTOS}\",
  \"build_type\": \"${CMAKE_BUILD_TYPE}\",
  \"compiler_id\": \"${CMAKE_C_COMPILER_ID}\",
  \"compiler_version\": \"${CMAKE_C_COMPILER_VERSION}\",
  \"compiler\": \"${CMAKE_C_COMPILER}\",
  \"ti_installer_sha256\": \"${SDOS_TI_INSTALLER_SHA256}\",
  \"container_image\": \"$ENV{SDOS_IMAGE}\"
}
")

message(STATUS "SDOS product=${SDOS_PRODUCT} core=${SDOS_CORE} rtos=${SDOS_RTOS} "
               "build=${CMAKE_BUILD_TYPE} compiler=${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
