# DSP-side code. firmware/common builds for every product: the DSPs run it, and
# the host library's mock links the same files, so PC tests exercise them.
add_subdirectory(common)

# Course firmware executables, one per firmware preset (core x runtime x build).
if(SDOS_PRODUCT STREQUAL "firmware")
  include(${PROJECT_SOURCE_DIR}/cmake/SdosTiFirmware.cmake)
  if(SDOS_CORE MATCHES "^c66x")
    set(family_sources c66x/src/c66x.c)
  else()
    set(family_sources c71x/src/c71x.c)
  endif()
  set(runtime_sources runtime/${SDOS_RTOS}/main.c)
  if(SDOS_RTOS STREQUAL "baremetal")
    list(APPEND runtime_sources runtime/baremetal/mailbox_poll.c)
  endif()
  sdos_add_ti_firmware(sdos_firmware SOURCES
    ${runtime_sources}
    common/platform/src/ipc_service.c
    common/platform/src/resource_table.c
    common/platform/src/trace.c
    common/platform/src/layout.c
    common/platform/src/monitor.c
    ${family_sources})
  target_include_directories(sdos_firmware PRIVATE common/platform/include)
endif()
