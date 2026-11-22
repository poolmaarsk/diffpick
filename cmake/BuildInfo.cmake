# Build information for Diffpick::BuildInfo:
#
#   BuildConfig.hpp  configure time: version of the application and of CMake
#   GitInfo.hpp      build time: git tag and commit, refreshed on every build
#
# Git data is regenerated at build time because a configure-time value goes
# stale after the next commit.

set(DIFFPICK_GENERATED_DIR "${PROJECT_BINARY_DIR}/generated")

configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/BuildConfig.hpp.in"
    "${DIFFPICK_GENERATED_DIR}/BuildConfig.hpp"
    @ONLY
)

find_package(Git QUIET)

add_custom_target(diffpick_git_info
    COMMAND "${CMAKE_COMMAND}"
        -DGIT_EXECUTABLE=${GIT_EXECUTABLE}
        -DSOURCE_DIR=${PROJECT_SOURCE_DIR}
        -DINPUT=${CMAKE_CURRENT_LIST_DIR}/GitInfo.hpp.in
        -DOUTPUT=${DIFFPICK_GENERATED_DIR}/GitInfo.hpp
        -P "${CMAKE_CURRENT_LIST_DIR}/scripts/GenerateGitInfo.cmake"
    BYPRODUCTS "${DIFFPICK_GENERATED_DIR}/GitInfo.hpp"
    COMMENT "Updating GitInfo.hpp"
    VERBATIM
)

add_library(diffpick_build_info INTERFACE)
add_library(Diffpick::BuildInfo ALIAS diffpick_build_info)
target_include_directories(diffpick_build_info INTERFACE "${DIFFPICK_GENERATED_DIR}")
add_dependencies(diffpick_build_info diffpick_git_info)
