cmake_minimum_required(VERSION 3.22)

include(cmake/DiffpickVersion.cmake)
project(diffpick VERSION ${DIFFPICK_VERSION} LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

include(CompilerCache)
include(ProjectOptions)
include(ProjectWarnings)
include(CTest)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core)
qt_standard_project_setup()

# Widget-free core logic, unit-tested with Qt Test.
add_library(diffpick_core STATIC
    src/PathUtil.hpp
    src/PathUtil.cpp
    src/MeldLauncher.hpp
    src/MeldLauncher.cpp
    src/PastedText.hpp
    src/PastedText.cpp
    src/Settings.hpp
    src/Settings.cpp
    src/Log.hpp
    src/Log.cpp
    src/Cli.hpp
    src/Cli.cpp
)
target_include_directories(diffpick_core PUBLIC src)
target_link_libraries(diffpick_core
    PUBLIC Qt6::Core
    PRIVATE Diffpick::ProjectOptions Diffpick::ProjectWarnings
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

include(Summary)
