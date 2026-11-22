find_package(Qt6 6.4 REQUIRED COMPONENTS Test)

foreach(test IN ITEMS tst_pathutil tst_meldargs tst_pastedtext tst_settings tst_log tst_cli)
    add_executable(${test} ${test}.cpp)
    target_link_libraries(${test} PRIVATE diffpick_core Qt6::Test
        Diffpick::ProjectOptions Diffpick::ProjectWarnings)
    add_test(NAME ${test} COMMAND ${test})

    # Put Qt's bin folder first in PATH so the test finds Qt and its compiler
    # runtime there, not an older copy elsewhere in PATH.
    if(WIN32)
        set_tests_properties(${test} PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
        )
    endif()
endforeach()

set(_src "${PROJECT_SOURCE_DIR}/src")
add_executable(tst_mainwindow tst_mainwindow.cpp
    ${_src}/MainWindow.hpp
    ${_src}/MainWindow.cpp
    ${_src}/SlotWidget.hpp
    ${_src}/SlotWidget.cpp
    ${_src}/TextDialog.hpp
    ${_src}/TextDialog.cpp
    ${_src}/SettingsDialog.hpp
    ${_src}/SettingsDialog.cpp
)
target_link_libraries(tst_mainwindow PRIVATE diffpick_core Qt6::Test Qt6::Widgets
    Diffpick::BuildInfo Diffpick::ProjectOptions Diffpick::ProjectWarnings)
add_test(NAME tst_mainwindow COMMAND tst_mainwindow)
set_tests_properties(tst_mainwindow PROPERTIES
    ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;QT_QPA_PLATFORM=set:offscreen"
)
if(WIN32)
    target_sources(tst_mainwindow PRIVATE ${_src}/IntegrationWin.cpp)
    target_link_libraries(tst_mainwindow PRIVATE ole32 advapi32)
elseif(UNIX AND NOT APPLE)
    target_sources(tst_mainwindow PRIVATE ${_src}/IntegrationLinux.cpp)
endif()
