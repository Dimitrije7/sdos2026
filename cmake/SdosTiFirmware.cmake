# Link course firmware against TI's installed PDK, the way TI's own PDK build
# links its IPC examples (captured from `make -n` of ipc_echo_test_freertos).
# Nothing TI ships is copied into the repository: headers, prebuilt libraries,
# and linker command files are read from SDOS_TI_PSDK_ROOT in the container.

set(_psdk "$ENV{SDOS_TI_PSDK_ROOT}")
file(GLOB _pdk LIST_DIRECTORIES true "${_psdk}/pdk_jacinto_*/packages")
if(NOT _pdk)
  message(FATAL_ERROR "firmware needs TI's PDK: SDOS_TI_PSDK_ROOT=${_psdk} has no pdk_jacinto_*/packages. "
                      "Run `make bootstrap TI=accept`.")
endif()
set(SDOS_PDK "${_pdk}" CACHE INTERNAL "")

# Per core: TI core name, build defines, IPC processor id, protocol core id.
set(_ti_core_c66x_0 c66xdsp_1)
set(_defines_c66x_0 BUILD_DSP_1 BUILD_C66X_1 BUILD_C66X SDOS_IPC_SELF=IPC_C66X_1 SDOS_CORE_ID=0)
set(_ti_core_c66x_1 c66xdsp_2)
set(_defines_c66x_1 BUILD_DSP_2 BUILD_C66X_2 BUILD_C66X SDOS_IPC_SELF=IPC_C66X_2 SDOS_CORE_ID=1)
set(_ti_core_c71x_0 c7x_1)
set(_defines_c71x_0 BUILD_C7X_1 BUILD_C7X SDOS_IPC_SELF=IPC_C7X_1 SDOS_CORE_ID=2)

set(SDOS_TI_CORE "${_ti_core_${SDOS_CORE}}" CACHE INTERNAL "")
if(SDOS_TI_FAMILY STREQUAL "c6000")
  set(_isa c66)
  set(_ext xe66)
  set(_lib_ext ae66)
else()
  set(_isa c7x)
  set(_ext xe71)
  set(_lib_ext ae71)
endif()

function(sdos_add_ti_firmware target)
  cmake_parse_arguments(ARG "" "" "SOURCES" ${ARGN})
  set(rtos_define FREERTOS)
  if(SDOS_RTOS STREQUAL "baremetal")
    set(rtos_define BAREMETAL)
  endif()
  set(pdk "${SDOS_PDK}")
  set(examples "${pdk}/ti/drv/ipc/examples/common/j721e/freertos")

  # Fault-injection builds (the *-fi presets) add algorithms that commit
  # faults on request, for the connected-board suite; never for course use.
  option(SDOS_FAULT_INJECTION "Build test firmware that can inject faults (docs/safety.md)" OFF)
  set(suffix "")
  set(label "${SDOS_RTOS}")
  if(SDOS_FAULT_INJECTION)
    list(APPEND ARG_SOURCES "${PROJECT_SOURCE_DIR}/firmware/common/platform/src/fault_injection.c")
    set(suffix "-fi")
    set(label "${SDOS_RTOS} FI")
  endif()
  add_executable(${target} ${ARG_SOURCES})
  if(SDOS_FAULT_INJECTION)
    target_compile_definitions(${target} PRIVATE SDOS_FAULT_INJECTION=1)
  endif()
  # The linked ELF keeps debug data, which differs between builds in a few
  # bytes; the stripped copy is what boards load, and it is reproducible.
  set(name "sdos-${SDOS_CORE}-${SDOS_RTOS}${suffix}")
  set_target_properties(${target} PROPERTIES OUTPUT_NAME "${name}.debug" SUFFIX ".${_ext}")
  target_compile_definitions(${target} PRIVATE
    SOC_J721E j721e_evm=j721e_evm ${rtos_define} ${_defines_${SDOS_CORE}}
    SDOS_VERSION_CODE=0x${PROJECT_VERSION_MAJOR}0${PROJECT_VERSION_MINOR}0${PROJECT_VERSION_PATCH}u
    "SDOS_FIRMWARE_NAME=\"sdos ${PROJECT_VERSION} ${label}\"")
  target_include_directories(${target} PRIVATE
    "${pdk}" "${pdk}/ti/kernel" "${examples}"
    "${pdk}/ti/kernel/freertos/FreeRTOS-LTS/FreeRTOS-Kernel/include"
    "${pdk}/ti/kernel/freertos/portable/TI_CGT/${_isa}"
    "${pdk}/ti/kernel/freertos/config/j721e/${_isa}")
  # TI's own flags for code that includes PDK headers; -pds silences the
  # three header diagnostics TI's build silences (238, 880, 1110).
  target_compile_options(${target} PRIVATE -pden -pds=238 -pds=880 -pds=1110)
  if(_isa STREQUAL "c7x")
    target_include_directories(${target} PRIVATE "${pdk}/ti/csl/arch/c7x")
  else()
    target_compile_options(${target} PRIVATE -mi10)
  endif()
  target_link_libraries(${target} PRIVATE sdos_runtime sdos_safety sdos_cores sdos_fir_q15)
  sdos_target_warnings(${target})

  # Libraries in TI's link order. TI ships release builds only; debug firmware
  # links them too and is debug in course code alone.
  set(core "${SDOS_TI_CORE}")
  set(libs
    "ti/csl/lib/j721e/${_isa}/release/ti.csl.${_lib_ext}"
    "ti/drv/uart/lib/j721e/${_isa}/release/ti.drv.uart.${_lib_ext}"
    "ti/drv/i2c/lib/j721e/${_isa}/release/ti.drv.i2c.${_lib_ext}"
    "ti/drv/udma/lib/j721e/${core}/release/udma.${_lib_ext}"
    "ti/drv/gpio/lib/j721e/${_isa}/release/ti.drv.gpio.${_lib_ext}"
    "ti/drv/pmic/lib/j721e_evm/${_isa}/release/pmic.${_lib_ext}"
    "ti/drv/pm/lib/j721e/${_isa}/release/pm_lib.${_lib_ext}"
    "ti/drv/gtc/lib/j721e/${_isa}/release/ti.drv.gtc.${_lib_ext}"
    "ti/board/lib/j721e_evm/${_isa}/release/ti.board.${_lib_ext}"
    "ti/drv/sciclient/lib/j721e/${core}/release/sciclient.${_lib_ext}")
  if(SDOS_RTOS STREQUAL "freertos")
    list(APPEND libs
      "ti/osal/lib/freertos/j721e/${_isa}/release/ti.osal.${_lib_ext}"
      "ti/kernel/lib/j721e/${core}/release/ti.kernel.freertos.${_lib_ext}"
      # TI's bare-metal IPC library under FreeRTOS too: it polls the mailbox
      # and asks the device manager for no interrupt route. The RTOS library
      # needs one, which this board's boot configuration refuses the C66x
      # (Task 021), and routes outlive a core restart. One IPC design for
      # every core and runtime.
      "ti/drv/ipc/lib/j721e/${core}/release/ipc_baremetal.${_lib_ext}")
  elseif(_isa STREQUAL "c66")
    list(APPEND libs
      "ti/osal/lib/nonos/j721e/c66/release/ti.osal.ae66"
      "ti/drv/ipc/lib/j721e/${core}/release/ipc_baremetal.ae66")
  else()
    # TI's bare-metal OSAL cannot register C7x interrupts, and the C7x boot
    # code (ti.csl.init) needs the interrupt and exception startup that only
    # TI's FreeRTOS port provides. Bare-metal C7x firmware therefore links the
    # FreeRTOS OSAL and port for startup alone; it never starts the scheduler.
    list(APPEND libs
      "ti/osal/lib/freertos/j721e/c7x/release/ti.osal.ae71"
      "ti/kernel/lib/j721e/${core}/release/ti.kernel.freertos.ae71"
      "ti/drv/ipc/lib/j721e/${core}/release/ipc_baremetal.ae71")
  endif()
  if(_isa STREQUAL "c66")
    list(APPEND libs "ti/csl/lib/j721e/c66/release/ti.csl.intc.ae66")
  else()
    list(APPEND libs "ti/csl/lib/j721e/c7x/release/ti.csl.init.ae71")
  endif()
  foreach(lib IN LISTS libs)
    if(NOT EXISTS "${pdk}/${lib}")
      message(FATAL_ERROR "TI library missing from the PDK: ${lib}")
    endif()
    target_link_libraries(${target} PRIVATE "${pdk}/${lib}")
  endforeach()

  # Linker command files (Task 041): the editable main file for this core,
  # which includes the generated sdos_memory.cmd, the runtime fragment, and
  # the module fragments. -DSDOS_LINKER_SCRIPT=<file> selects another layout.
  set(SDOS_LINKER_SCRIPT "${PROJECT_SOURCE_DIR}/firmware/linker/${SDOS_CORE}/SDOS.cmd" CACHE FILEPATH
      "Main linker command file for this firmware build")
  get_filename_component(script_dir "${SDOS_LINKER_SCRIPT}" DIRECTORY)
  string(TOUPPER "${SDOS_RTOS}" rtos_upper)
  set(model --rom_model)
  if(_isa STREQUAL "c7x")
    set(model --ram_model)
  endif()
  get_filename_component(memory_dir "${SDOS_MEMORY_CMD}" DIRECTORY)
  target_link_options(${target} PRIVATE
    # A linker warning can mean an image that cannot run (a relocation
    # overflow, an unplaced section), so every link treats warnings as errors.
    --emit_warnings_as_errors --warn_sections --reread_libs ${model} "--define=SDOS_RTOS_${rtos_upper}"
    "-i${script_dir}" "-i${memory_dir}" "-i${PROJECT_SOURCE_DIR}/firmware/linker/common"
    "-i${PROJECT_SOURCE_DIR}/firmware/linker/modules" "${SDOS_LINKER_SCRIPT}"
    "-i${SDOS_TI_ROOT}/lib")
  file(GLOB fragments "${PROJECT_SOURCE_DIR}/firmware/linker/*/*.cmd")
  set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS "${SDOS_LINKER_SCRIPT};${SDOS_MEMORY_CMD};${fragments}")
  # libc.a is TI's index of runtime libraries; it must come after every object.
  target_link_libraries(${target} PRIVATE "${SDOS_TI_ROOT}/lib/libc.a")

  # Every build is checked against the core's carve-out and resource table.
  if(_isa STREQUAL "c66")
    set(strip "${SDOS_TI_ROOT}/bin/strip6x")
  else()
    set(strip "${SDOS_TI_ROOT}/bin/strip7x")
  endif()
  set(stripped "$<TARGET_FILE_DIR:${target}>/${name}.${_ext}")
  # The post-link step enforces the protection rules and writes the integrity
  # record into the linked ELF before it is stripped (docs/safety.md).
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND python3 "${PROJECT_SOURCE_DIR}/ci/firmware_postlink.py" "${SDOS_CORE}" "${SDOS_RTOS}"
            "$<TARGET_FILE:${target}>"
    COMMAND "${strip}" -p "$<TARGET_FILE:${target}>" -o "${stripped}"
    COMMAND python3 "${PROJECT_SOURCE_DIR}/ci/firmware_elf.py" --require-trace --require-integrity
            --runtime "${SDOS_RTOS}" "${SDOS_CORE}" "${stripped}"
    COMMAND python3 "${PROJECT_SOURCE_DIR}/ci/linker_report.py" "${CMAKE_BINARY_DIR}/${target}.map"
            "$<TARGET_FILE_DIR:${target}>/${name}"
    COMMENT "Checking, stripping, and reporting ${name}.${_ext} against ${SDOS_CORE}'s memory map")
endfunction()
