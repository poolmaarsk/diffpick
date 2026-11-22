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
include(BuildInfo)
include(CTest)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets)
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

qt_add_executable(diffpick WIN32
    assets/diffpick.qrc
    src/Main.cpp
    src/MainWindow.hpp
    src/MainWindow.cpp
    src/SlotWidget.hpp
    src/SlotWidget.cpp
    src/TextDialog.hpp
    src/TextDialog.cpp
    src/SettingsDialog.hpp
    src/SettingsDialog.cpp
    src/Integration.hpp
)
target_link_libraries(diffpick PRIVATE
    diffpick_core Qt6::Gui Qt6::Widgets
    Diffpick::BuildInfo Diffpick::ProjectOptions Diffpick::ProjectWarnings
)
set_target_properties(diffpick PROPERTIES AUTORCC ON)
if(WIN32)
    configure_file(assets/diffpick.rc.in generated/diffpick.rc @ONLY)
    target_sources(diffpick PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated/diffpick.rc"
        src/IntegrationWin.cpp)
    set_property(SOURCE "${CMAKE_CURRENT_BINARY_DIR}/generated/diffpick.rc"
        APPEND PROPERTY OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/diffpick.ico")
    target_link_libraries(diffpick PRIVATE ole32 advapi32)
elseif(UNIX AND NOT APPLE)
    target_sources(diffpick PRIVATE src/IntegrationLinux.cpp)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

include(Summary)
