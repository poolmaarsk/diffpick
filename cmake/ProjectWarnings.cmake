# Warning flags for the project sources (not for Qt: its headers are system
# includes). Link Diffpick::ProjectWarnings to a target to apply them.

option(DIFFPICK_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

add_library(diffpick_project_warnings INTERFACE)
add_library(Diffpick::ProjectWarnings ALIAS diffpick_project_warnings)

set(_diffpick_gnu_warnings
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wnon-virtual-dtor
    -Woverloaded-virtual
    -Wcast-align
    -Wformat=2
)

target_compile_options(diffpick_project_warnings INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:${_diffpick_gnu_warnings}>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

if(DIFFPICK_WARNINGS_AS_ERRORS)
    target_compile_options(diffpick_project_warnings INTERFACE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Werror>
        $<$<CXX_COMPILER_ID:MSVC>:/WX>
    )
endif()
