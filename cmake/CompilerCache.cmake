# Use sccache or ccache as the compiler launcher when installed. Include
# before any target is created.

option(DIFFPICK_COMPILER_CACHE "Use sccache/ccache when available" ON)

if(DIFFPICK_COMPILER_CACHE AND NOT CMAKE_CXX_COMPILER_LAUNCHER)
    set(_diffpick_cache_names sccache ccache)
    if(MSVC)
        set(_diffpick_cache_names sccache)
    endif()

    find_program(DIFFPICK_COMPILER_CACHE_PROGRAM NAMES ${_diffpick_cache_names})

    if(DIFFPICK_COMPILER_CACHE_PROGRAM)
        set(CMAKE_CXX_COMPILER_LAUNCHER "${DIFFPICK_COMPILER_CACHE_PROGRAM}")
        message(STATUS "Compiler cache: ${DIFFPICK_COMPILER_CACHE_PROGRAM}")
    else()
        message(STATUS "Compiler cache: not found (install sccache or ccache)")
    endif()
endif()
