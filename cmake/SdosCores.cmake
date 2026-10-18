# Generate sdos/cores.h from manifest/dsp-cores.json, the single source of DSP
# identities and carve-outs. Target sdos_cores carries the include directory.

set(_manifest "${PROJECT_SOURCE_DIR}/manifest/dsp-cores.json")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_manifest}")
file(READ "${_manifest}" _json)
string(JSON _count LENGTH "${_json}" cores)
math(EXPR _last "${_count} - 1")

set(_rows "")
set(_macros "")
foreach(_i RANGE ${_last})
  foreach(_key name label family device kernel_firmware)
    string(JSON _${_key} GET "${_json}" cores ${_i} ${_key})
  endforeach()
  foreach(_region firmware dma_pool)
    string(JSON _${_region}_base GET "${_json}" cores ${_i} memory ${_region} base)
    string(JSON _${_region}_size GET "${_json}" cores ${_i} memory ${_region} size)
  endforeach()
  if(_family STREQUAL "c6000")
    set(_machine 140)
    set(_ext xe66)
  else()
    set(_machine 145)
    set(_ext xe71)
  endif()
  string(TOUPPER "${_name}" _upper)
  string(APPEND _macros
    "#define SDOS_${_upper}_FIRMWARE_BASE ${_firmware_base}u\n"
    "#define SDOS_${_upper}_FIRMWARE_SIZE ${_firmware_size}u\n"
    "#define SDOS_${_upper}_DMA_POOL_BASE ${_dma_pool_base}u\n"
    "#define SDOS_${_upper}_DMA_POOL_SIZE ${_dma_pool_size}u\n")
  string(APPEND _rows
    "    {\"${_name}\", \"${_label}\", \"${_device}\", \"${_kernel_firmware}\", ${_machine}u, \"${_ext}\",\n"
    "     ${_firmware_base}u, ${_firmware_size}u, ${_dma_pool_base}u, ${_dma_pool_size}u},\n")
endforeach()

set(_out "${CMAKE_BINARY_DIR}/generated/include/sdos/cores.h")
file(CONFIGURE OUTPUT "${_out}" @ONLY CONTENT
"/* Generated from manifest/dsp-cores.json by cmake/SdosCores.cmake. Do not edit. */
#ifndef SDOS_CORES_H
#define SDOS_CORES_H

#include <stdint.h>

typedef struct sdos_core_desc {
    const char *name;            /* logical name, e.g. c66x_0 */
    const char *label;           /* display name, e.g. C66x_0 */
    const char *device;          /* device-tree unit address, the stable identity */
    const char *kernel_firmware; /* the name the kernel requests at boot */
    uint16_t elf_machine;        /* EM_TI_C6000 or EM_TI_C7X */
    const char *firmware_ext;
    uint32_t firmware_base;      /* carve-out every loadable segment must stay in */
    uint32_t firmware_size;
    uint32_t dma_pool_base;      /* vrings and shared buffers */
    uint32_t dma_pool_size;
} sdos_core_desc;

#define SDOS_CORE_COUNT @_count@

/* The same values as macros, for preprocessor checks in firmware. */
@_macros@

#ifndef SDOS_CORES_MACROS_ONLY /* firmware needs only the macros below */
static const sdos_core_desc sdos_core_table[SDOS_CORE_COUNT] = {
@_rows@};
#endif

#endif /* SDOS_CORES_H */
")

add_library(sdos_cores INTERFACE)
target_include_directories(sdos_cores INTERFACE "${CMAKE_BINARY_DIR}/generated/include")
