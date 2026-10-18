# Warning policy for SDOS targets. `make quality` configures with SDOS_WERROR=ON.

function(sdos_target_warnings target)
  if(CMAKE_C_COMPILER_ID MATCHES "^(GNU|Clang)$")
    target_compile_options(${target} PRIVATE
      -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow
      -Wstrict-prototypes -Wmissing-prototypes -Wcast-qual -Wundef)
    if(SDOS_WERROR)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  elseif(CMAKE_C_COMPILER_ID STREQUAL "TI")
    # --display_error_number makes diagnostics citable; --emit_warnings_as_errors
    # is the TI equivalent of -Werror.
    target_compile_options(${target} PRIVATE --display_error_number)
    if(SDOS_WERROR)
      target_compile_options(${target} PRIVATE --emit_warnings_as_errors)
    endif()
  endif()
endfunction()
