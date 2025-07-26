
# Convenience macro to add test programs
# Usage: add_test_executable(NAME name FILES file...
#                            [COMMAND command...] 
#                            [CLI_ARGS arg...])
#
# This will compile the listed files and make an executable.
# The executable is invoked when running the test case with ctest.
#
# Additional command line arguments to the executable can be specified
# with the CLI_ARGS argument.
#
# By default, this macro creates a with running the executable as the
# test command. This can be overridden by specifying COMMAND, for example
# if the test is to run your program using `time`. Note that the name of 
# the executable is prefixed with "test_". Also note that by setting COMMAND,
# CLI_ARGS are ignored.

macro(add_test_executable)
    cmake_parse_arguments(ARG "" "NAME;COMMAND" "FILES;CLI_ARGS" ${ARGN})

    if(NOT ARG_NAME)
        message(FATAL_ERROR "add_test_executable must have a non-empty NAME")
    endif()

    string(FIND "${ARG_NAME}" "test_" _prefix_pos)
    if(NOT _prefix_pos EQUAL 0)
        string(PREPEND ARG_NAME "test_")
    endif()

    if(NOT ARG_FILES)
        message("add_test_executable expects a non-empty FILES")
    endif()

    if(NOT ARG_COMMAND)
        set(ARG_COMMAND "${ARG_NAME}" ${ARG_CLI_ARGS})
    elseif(ARG_CLI_ARGS)
        message(WARNING "add_test_executable is invoked with COMMAND, CLI_ARGS is ignored.")
    endif()

    if(BUILD_TESTING)
        add_executable(${ARG_NAME} ${ARG_FILES})
        target_link_libraries(${ARG_NAME} utilslib)
        add_test(NAME ${ARG_NAME} COMMAND ${ARG_COMMAND})
    endif()
endmacro()


# Add test directories
add_subdirectory(rbtree)
