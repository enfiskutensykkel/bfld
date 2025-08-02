include(CTest)

# Convenience macro to add test programs
# Usage: add_test_executable(target_name
#                            FILES file...
#                            [OUTPUT_NAME name]
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
# if the test is to run your program using `time`. Note that by setting 
# COMMAND, CLI_ARGS are ignored.

macro(add_test_executable target_name)
    cmake_parse_arguments(ARG "" "OUTPUT_NAME;COMMAND" "FILES;CLI_ARGS" ${ARGN})

    if("${ARG_OUTPUT_NAME}" STREQUAL "")
        set(ARG_OUTPUT_NAME "${target_name}")
    endif()

    if(NOT ARG_FILES)
        message("add_test_executable expects a non-empty FILES")
    endif()

    if(NOT ARG_COMMAND)
        set(ARG_COMMAND "${ARG_OUTPUT_NAME}" ${ARG_CLI_ARGS})
    elseif(ARG_CLI_ARGS)
        message(WARNING "add_test_executable is invoked with COMMAND, CLI_ARGS is ignored.")
    endif()

    if(BUILD_TESTING)
        add_executable(${target_name} ${ARG_FILES})
        set_target_properties(${target_name} PROPERTIES OUTPUT_NAME ${ARG_OUTPUT_NAME})
        add_test(NAME ${target_name} COMMAND ${ARG_COMMAND})
    else()
        add_custom_target(${ARG_NAME})
    endif()
endmacro()


# Add test directories
add_subdirectory(rbtree)
