# Shared compile flags for the dependency-free DSP core and its tests.
# The core must stay portable to a bare-metal Cortex-M7 build, so it is
# compiled without exceptions or RTTI and with double promotion as an error.
add_library(hic_warnings INTERFACE)
if(MSVC)
  target_compile_options(hic_warnings INTERFACE /W4 /fp:precise)
else()
  target_compile_options(hic_warnings INTERFACE
    -Wall -Wextra -Wshadow -Wconversion -Wsign-conversion
    -Wdouble-promotion -Werror=double-promotion
    -fno-math-errno -fno-trapping-math)
endif()

add_library(hic_core_flags INTERFACE)
if(NOT MSVC)
  target_compile_options(hic_core_flags INTERFACE -fno-exceptions -fno-rtti)
endif()
