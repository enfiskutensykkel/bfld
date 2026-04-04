cmake_minimum_required(VERSION 3.16...3.30)
project(bfld)

find_package(Threads REQUIRED)
include(CheckIncludeFile)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_C_STANDARD_REQUIRED ON)

check_include_file("valgrind/memcheck.h" HAVE_VALGRIND_MEMCHECK_H)

add_compile_options("$<$<CONFIG:TSan>:-fsanitize=thread;-g;-O1>")
add_link_options("$<$<CONFIG:TSan>:-fsanitize=thread>")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Set build type" FORCE)
endif()

if (HAVE_VALGRIND_MEMCHECK_H)
    add_compile_definitions(HAS_VALGRIND)
    message(STATUS "valgrind headers detected")
endif ()

file(GLOB_RECURSE BFLD_HEADER_FILES "include/*.h")


# Compile a utility library for (maybe useful for other projects)?
add_library(utilslib SHARED EXCLUDE_FROM_ALL 
    src/utils/rbtree.c 
    src/utils/deque.c 
    src/utils/table.c 
    src/utils/arena.c
    src/utils/htable.c
)
target_sources(utilslib INTERFACE 
    include/utils/spinlock.h 
    include/utils/list.h 
    include/utils/rbtree.h 
    include/utils/deque.h 
    include/utils/table.h 
    include/utils/hash.h 
    include/utils/arena.h 
    include/utils/rht.h
    include/utils/htable.h
)
target_include_directories(utilslib PUBLIC include/utils)
target_compile_options(utilslib PRIVATE -Wall -Wextra -pedantic)
set_target_properties(utilslib PROPERTIES OUTPUT_NAME bfldutils)

#install(TARGETS utilslib DESTINATION lib)
#file(GLOB UTILSLIB_HEADER_FILES "include/utils/*.h")
#install(FILES ${UTILSLIB_HEADER_FILES} DESTINATION include/bfldutils)

string(TOUPPER ${CMAKE_BUILD_TYPE} BUILD_TYPE_UPPER)

# Should we build tests?
option(BUILD_TESTING "Build tests for bfld" OFF)
if ("${BUILD_TYPE_UPPER}" STREQUAL "DEBUG" OR "${BUILD_TYPE_UPPER}" STREQUAL "TSAN")
    set(BUILD_TESTING ON FORCE)
endif ()


# Add test directory
if (BUILD_TESTING)
    if (NOT "${BUILD_TYPE_UPPER}" STREQUAL "DEBUG" AND NOT "${BUILD_TYPE_UPPER}" STREQUAL "TSAN")
        message(STATUS "BUILD_TESTING is set but build type is '${CMAKE_BUILD_TYPE}'")
    endif ()
    enable_testing()
    add_subdirectory(tests)
endif ()


# Create shared library of the linker code
add_library(linkerlib SHARED
    src/linker/strpool.c
)
#add_library(linkerlib SHARED 
#    src/linker/strpool.c
#    src/utils/deque.c
#    src/utils/table.c
#    src/utils/rbtree.c
#    src/linker/mfile.c 
#    src/linker/registry.c
#    src/linker/linker.c
#    src/linker/objectfile.c
#    src/linker/archive.c
#    src/linker/archives.c
#    src/linker/section.c
#    src/linker/sections.c
#    src/linker/symbol.c
#    src/linker/symbols.c
#    src/linker/globals.c
#    #src/linker/layout.c
#    src/linker/posixcompat.c
#    src/linker/groups.c
#)
target_include_directories(linkerlib PUBLIC include)  # also includes "include/utils"
target_include_directories(linkerlib PRIVATE include/utils)
target_compile_options(linkerlib PRIVATE -Wall -Wextra -pedantic)
set_target_properties(linkerlib PROPERTIES OUTPUT_NAME bfld)


# Make an executable linker

# TODO: at some point we want to register loaders/frontends with dlsym/dlopen and determine loaders/frontends with command line arguments (then point default loader based on architecture using if (macos/unix)

add_executable(bfld src/main.c src/commandline.c)
add_subdirectory(src/frontends)
add_subdirectory(src/targets)

#target_compile_definitions(bfld PUBLIC DEFAULT_BFVM="$<TARGET_FILE:bfvm>")
target_compile_definitions(bfld PUBLIC DEFAULT_BFVM="NOFILE")
target_compile_options(bfld PRIVATE -Wall -Wextra -pedantic)
target_link_libraries(bfld PRIVATE linkerlib)
