# TI C7000 compiler 5.0.0.LTS for the C71x DSP cores. The compiler is not in this
# repository or the open container: `make bootstrap` downloads TI's installer,
# verifies it against manifest/versions.json, and sets SDOS_TI_C7000_ROOT.
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR c7000)
set(SDOS_TI_FAMILY c7000 CACHE INTERNAL "")
set(SDOS_TI_C7000_VERSION "5.0.0.LTS")

set(_root "$ENV{SDOS_TI_C7000_ROOT}")
if(NOT _root OR NOT EXISTS "${_root}/bin/cl7x")
  message(FATAL_ERROR
    "TI C7000 compiler 5.0.0.LTS not found: set SDOS_TI_C7000_ROOT to its install "
    "directory. The TI toolchain layer that installs it is Task 019; until then "
    "firmware presets cannot be configured.")
endif()

set(CMAKE_C_COMPILER "${_root}/bin/cl7x")
set(CMAKE_AR "${_root}/bin/ar7x")
# Target flags match the compile proven by Task 005.
set(CMAKE_C_FLAGS_INIT "-mv7100 --c99 -I${_root}/include")
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(SDOS_PRODUCT firmware CACHE STRING "" FORCE)
