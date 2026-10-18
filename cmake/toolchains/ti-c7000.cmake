# TI C7000 compiler 5.0.0.LTS for the C71x DSP cores. The compiler is not in this
# repository or the open container: `make bootstrap` downloads TI's installer,
# verifies it against manifest/versions.json, and sets SDOS_TI_C7000_ROOT.
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR c7000)
set(SDOS_TI_FAMILY c7000 CACHE INTERNAL "")
set(SDOS_TI_EXPECTED_VERSION "5.0.0.LTS" CACHE STRING "TI compiler version pinned by the manifest")

set(_root "$ENV{SDOS_TI_C7000_ROOT}")
if(NOT _root OR NOT EXISTS "${_root}/bin/cl7x")
  message(FATAL_ERROR
    "TI C7000 compiler ${SDOS_TI_EXPECTED_VERSION} not found: set SDOS_TI_C7000_ROOT to its install "
    "directory. The TI toolchain layer that installs it is Task 019; until then "
    "firmware presets cannot be configured.")
endif()

set(CMAKE_C_COMPILER "${_root}/bin/cl7x")
set(CMAKE_AR "${_root}/bin/ar7x")
# Target flags match the compile proven by Task 005.
# --silicon_errata_i2117 works around C7x erratum i2117; TI builds every C7x
# library with it, so all C7x code here does too.
set(CMAKE_C_FLAGS_INIT "-mv7100 --c99 --silicon_errata_i2117 -I${_root}/include")
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(SDOS_TI_ROOT "${_root}" CACHE INTERNAL "TI compiler install directory")
set(SDOS_PRODUCT firmware CACHE STRING "" FORCE)
# The container records the SHA-256 of the verified installer next to the install.
if(EXISTS "${_root}.installer.sha256")
  file(READ "${_root}.installer.sha256" _sha)
  string(STRIP "${_sha}" SDOS_TI_INSTALLER_SHA256)
endif()
