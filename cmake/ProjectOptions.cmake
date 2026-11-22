# Language requirements of the project sources.

set(CMAKE_CXX_EXTENSIONS OFF)

add_library(diffpick_project_options INTERFACE)
add_library(Diffpick::ProjectOptions ALIAS diffpick_project_options)
target_compile_features(diffpick_project_options INTERFACE cxx_std_17)
