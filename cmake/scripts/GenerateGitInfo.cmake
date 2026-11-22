# Script mode (cmake -P), run on every build by the diffpick_git_info target.
# Inputs: GIT_EXECUTABLE, SOURCE_DIR, INPUT, OUTPUT.
#
# GIT_TAG     nearest tag, or "no-tag"
# GIT_COMMIT  short hash with a "-dirty" suffix when tracked files have changes,
#             or "unknown" without git or without a commit

set(GIT_TAG "unknown")
set(GIT_COMMIT "unknown")

if(GIT_EXECUTABLE)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" describe --tags --abbrev=0
        WORKING_DIRECTORY "${SOURCE_DIR}"
        OUTPUT_VARIABLE GIT_TAG
        RESULT_VARIABLE _tag_result
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(NOT _tag_result EQUAL 0)
        set(GIT_TAG "no-tag")
    endif()

    execute_process(
        COMMAND "${GIT_EXECUTABLE}" describe --always --dirty --abbrev=8 --exclude=*
        WORKING_DIRECTORY "${SOURCE_DIR}"
        OUTPUT_VARIABLE GIT_COMMIT
        RESULT_VARIABLE _commit_result
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(NOT _commit_result EQUAL 0)
        set(GIT_COMMIT "unknown")
    endif()
endif()

# configure_file() leaves OUTPUT untouched when the content is unchanged, so a
# build without a new commit recompiles nothing.
configure_file("${INPUT}" "${OUTPUT}" @ONLY)
message(STATUS "Git: tag ${GIT_TAG}, commit ${GIT_COMMIT}")
