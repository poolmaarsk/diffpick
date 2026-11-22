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
