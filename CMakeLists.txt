cmake_minimum_required(VERSION 3.16...3.30)
project(bfld)

file(GLOB_RECURSE BFLD_HEADER_FILES "include/*.h")


# Compile a utility library for (maybe useful for other projects)?
add_library(utilslib SHARED EXCLUDE_FROM_ALL src/utils/rbtree.c)
target_sources(utilslib INTERFACE include/utils/list.h include/utils/rbtree.h)
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

# TODO: at some point we want to register loaders/frontends with dlsym/dlopen and determine loaders/frontends with command line arguments (then point default loader based on architecture using if (macos/unix)

add_executable(bfld src/main.c)
add_subdirectory(src/elf)

#target_compile_definitions(bfld PUBLIC DEFAULT_BFVM="$<TARGET_FILE:bfvm>")
target_compile_definitions(bfld PUBLIC DEFAULT_BFVM="NOFILE")
target_compile_options(bfld PRIVATE -Wall -Wextra -pedantic)
target_link_libraries(bfld PRIVATE linkerlib)
