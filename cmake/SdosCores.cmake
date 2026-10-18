# Generate DSP identity and memory definitions from manifest/dsp-cores.json,
# the single source of hardware facts (Task 041):
#   sdos/cores.h       C table of cores and of the ranges remoteproc may load
#   sdos_memory.cmd    TI linker macros for the firmware preset's core and runtime
# Section placement stays in the editable linker files under firmware/linker/.

set(_manifest "${PROJECT_SOURCE_DIR}/manifest/dsp-cores.json")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_manifest}")
file(READ "${_manifest}" _json)
string(JSON _count LENGTH "${_json}" cores)
math(EXPR _last "${_count} - 1")

set(_rows "")
set(_ranges "")
set(_range_count 0)
set(_macros "")
set(_linker "")
foreach(_i RANGE ${_last})
  foreach(_key name label family device kernel_firmware boot_alignment)
    string(JSON _${_key} GET "${_json}" cores ${_i} ${_key})
  endforeach()
  string(TOUPPER "${_name}" _upper)
  if(_family STREQUAL "c6000")
    set(_machine 140)
    set(_ext xe66)
  else()
    set(_machine 145)
    set(_ext xe71)
  endif()
  string(APPEND _rows "    {\"${_name}\", \"${_label}\", \"${_device}\", \"${_kernel_firmware}\", ${_machine}u, \"${_ext}\", ${_boot_alignment}u},\n")

  string(JSON _nregions LENGTH "${_json}" cores ${_i} regions)
  math(EXPR _rlast "${_nregions} - 1")
  foreach(_r RANGE ${_rlast})
    foreach(_key name base size loadable runtime_use owner)
      string(JSON _r_${_key} GET "${_json}" cores ${_i} regions ${_r} ${_key})
    endforeach()
    string(TOUPPER "${_r_name}" _rupper)
    # Cache reserve per runtime; the C loader table takes the smallest, since
    # the board does not know which runtime a firmware uses.
    set(_reserve_min 0)
    set(_reserve_this 0)
    string(JSON _reserves ERROR_VARIABLE _none GET "${_json}" cores ${_i} regions ${_r} cache_reserve)
    if(NOT _none)
      set(_first TRUE)
      foreach(_rt freertos baremetal)
        string(JSON _v GET "${_reserves}" ${_rt})
        math(EXPR _v "${_v}" OUTPUT_FORMAT DECIMAL)
        if(_first OR _v LESS _reserve_min)
          set(_reserve_min ${_v})
          set(_first FALSE)
        endif()
        if(_rt STREQUAL SDOS_RTOS)
          set(_reserve_this ${_v})
        endif()
      endforeach()
    endif()
    math(EXPR _usable "${_r_size} - ${_reserve_this}" OUTPUT_FORMAT HEXADECIMAL)
    math(EXPR _loadable_size "${_r_size} - ${_reserve_min}" OUTPUT_FORMAT HEXADECIMAL)
    math(EXPR _loadable_dec "${_r_size} - ${_reserve_min}")
    string(APPEND _macros "#define SDOS_${_upper}_${_rupper}_BASE ${_r_base}u\n"
                          "#define SDOS_${_upper}_${_rupper}_SIZE ${_r_size}u\n")
    if(_r_loadable AND _r_runtime_use STREQUAL "any" AND _loadable_dec GREATER 0)
      string(APPEND _ranges "    {${_i}u, ${_r_base}u, ${_loadable_size}u},\n")
      math(EXPR _range_count "${_range_count} + 1")
    endif()
    if(_name STREQUAL SDOS_CORE)
      string(APPEND _linker
        "/* ${_r_name}: owner ${_r_owner}, loadable ${_r_loadable}, use ${_r_runtime_use} */\n"
        "#define SDOS_${_rupper}_ORIGIN ${_r_base}\n"
        "#define SDOS_${_rupper}_LENGTH ${_r_size}\n"
        "#define SDOS_${_rupper}_USABLE ${_usable}\n")
    endif()
  endforeach()
endforeach()

set(_out "${CMAKE_BINARY_DIR}/generated/include/sdos/cores.h")
file(CONFIGURE OUTPUT "${_out}" @ONLY CONTENT
"/* Generated from manifest/dsp-cores.json by cmake/SdosCores.cmake. Do not edit. */
#ifndef SDOS_CORES_H
#define SDOS_CORES_H

#include <stdint.h>

#define SDOS_CORE_COUNT @_count@
#define SDOS_LOAD_RANGE_COUNT @_range_count@

/* Region bases and sizes as macros, for preprocessor checks in firmware. */
@_macros@
#ifndef SDOS_CORES_MACROS_ONLY /* firmware needs only the macros above */
typedef struct sdos_core_desc {
    const char *name;            /* logical name, e.g. c66x_0 */
    const char *label;           /* display name, e.g. C66x_0 */
    const char *device;          /* device-tree unit address, the stable identity */
    const char *kernel_firmware; /* the name the kernel requests at boot */
    uint16_t elf_machine;        /* EM_TI_C6000 or EM_TI_C7X */
    const char *firmware_ext;
    uint32_t boot_alignment;     /* the entry point must be a multiple of this */
} sdos_core_desc;

/* Where remoteproc may load a segment for a core, with the smallest cache
 * reserve of any runtime; the build checks each runtime exactly. */
typedef struct sdos_load_range {
    uint32_t core; /* index in sdos_core_table */
    uint32_t base;
    uint32_t size;
} sdos_load_range;

static const sdos_core_desc sdos_core_table[SDOS_CORE_COUNT] = {
@_rows@};

static const sdos_load_range sdos_load_ranges[SDOS_LOAD_RANGE_COUNT] = {
@_ranges@};
#endif

#endif /* SDOS_CORES_H */
")

if(SDOS_PRODUCT STREQUAL "firmware")
  set(SDOS_MEMORY_CMD "${CMAKE_BINARY_DIR}/generated/linker/sdos_memory.cmd" CACHE INTERNAL "")
  file(CONFIGURE OUTPUT "${SDOS_MEMORY_CMD}" @ONLY CONTENT
"/* Generated from manifest/dsp-cores.json by cmake/SdosCores.cmake for
 * @SDOS_CORE@ with the @SDOS_RTOS@ runtime. Do not edit: change the manifest.
 *
 * ORIGIN and LENGTH are the hardware region; USABLE is LENGTH minus what this
 * runtime configures as cache. A region's end address is ORIGIN + USABLE - 1.
 * The validator (ci/firmware_elf.py) enforces ownership and loadability. */
@_linker@")
endif()

add_library(sdos_cores INTERFACE)
target_include_directories(sdos_cores INTERFACE "${CMAKE_BINARY_DIR}/generated/include")
