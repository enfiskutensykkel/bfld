cmake_minimum_required(VERSION 3.16...3.30)
project(bfld)


# Compile the virtual machine
add_library(bfvm STATIC src/vm/bfvm.c src/vm/foo.c)
# TODO: do something similar to bfld, make a lib that's just the vm and then make an executable in order to run the VM from cli


# Create a generic library of the linker
add_library(libbfld SHARED src/lib/mfile.c src/lib/objfile.c)
target_include_directories(libbfld PUBLIC include)
target_include_directories(libbfld PUBLIC src/utils)
target_compile_definitions(libbfld PUBLIC DEFAULT_BFVM="$<TARGET_FILE:bfvm>")
target_compile_options(libbfld PRIVATE -Wall -Wextra -pedantic -g -ggdb)
set_target_properties(libbfld PROPERTIES OUTPUT_NAME bfld)


# Add OS specific sources to the target
if (UNIX)
    message(STATUS "Compiling for Linux")
    target_sources(libbfld PRIVATE src/lib/linux/ar.c src/lib/linux/elf.c)

else ()
    message(FATAL_ERROR "Platform is not supported")

endif ()


# Create executable linker
add_executable(bfld src/linker/main.c)
target_link_libraries(bfld PRIVATE libbfld)
