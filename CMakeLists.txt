cmake_minimum_required(VERSION 3.16...3.30)
project(bfld)

file(GLOB_RECURSE BFLD_HEADER_FILES "include/*.h")


# Compile a utility library for (maybe useful for other projects)?
add_library(utilslib SHARED src/utils/rbtree.c)
target_include_directories(utilslib PUBLIC include/utils)
target_compile_options(utilslib PRIVATE -Wall -Wextra -pedantic)
set_target_properties(utilslib PROPERTIES OUTPUT_NAME bfldutils)

#install(TARGETS utilslib DESTINATION lib)
#file(GLOB UTILSLIB_HEADER_FILES "include/utils/*.h")
#install(FILES ${UTILSLIB_HEADER_FILES} DESTINATION include/bfldutils)

# Should we build tests?
option(BUILD_TESTING "Build tests for bfld" OFF)

# Add test directory
if (BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif ()


# Create shared library of the linker code
add_library(linkerlib SHARED 
    src/utils/rbtree.c
    src/linker/mfile.c 
    src/linker/objfile.c 
    src/linker/section.c
    src/linker/symbol.c
)
target_include_directories(linkerlib PUBLIC include)  # also includes "include/utils"
target_include_directories(linkerlib PRIVATE include/utils)
target_compile_options(linkerlib PRIVATE -Wall -Wextra -pedantic)
set_target_properties(linkerlib PROPERTIES OUTPUT_NAME bfld)


# Make an executable linker
add_executable(bfld src/main.c)
#target_compile_definitions(bfld PUBLIC DEFAULT_BFVM="$<TARGET_FILE:bfvm>")
target_compile_definitions(bfld PUBLIC DEFAULT_BFVM="NOFILE")
target_compile_options(bfld PRIVATE -Wall -Wextra -pedantic)
target_link_libraries(bfld PRIVATE linkerlib)


## Compile the virtual machine
#add_library(bfvm STATIC src/vm/bfvm.c src/vm/foo.c)
## TODO: do something similar to bfld, make a lib that's just the vm and then make an executable in order to run the VM from cli
#
#
## Create a generic library of the linker
#add_library(libbfld SHARED src/lib/mfile.c src/lib/objfile.c src/lib/symbol.c)
#target_link_libraries(libbfld PRIVATE bfldutils)
#target_include_directories(libbfld PUBLIC include)
#target_compile_definitions(libbfld PUBLIC DEFAULT_BFVM="$<TARGET_FILE:bfvm>")
#target_compile_options(libbfld PRIVATE -Wall -Wextra -pedantic -g -ggdb)
#set_target_properties(libbfld PROPERTIES OUTPUT_NAME bfld)
#
#
## Add OS specific sources to the target
#if (UNIX)
#    message(STATUS "Compiling for Linux")
#    target_sources(libbfld PRIVATE src/lib/linux/ar.c src/lib/linux/elf.c)
#
#else ()
#    message(FATAL_ERROR "Platform is not supported")
#
#endif ()
#
#
## Create executable linker
#add_executable(bfld src/linker/main.c)
#target_link_libraries(bfld PRIVATE libbfld)


