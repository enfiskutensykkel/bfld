cmake_minimum_required(VERSION 3.16...3.30)
project(bfld)

# Compile and link the BF virtual machine
add_library(bfvm STATIC src/vm/bfvm.c src/vm/foo.c)

# Create target for the linker
add_executable(bfld src/linker/io.c src/linker/main.c src/linker/image.c src/linker/inputobj.c)
target_include_directories(bfld PUBLIC include)
target_compile_definitions(bfld PUBLIC DEFAULT_BFVM="$<TARGET_FILE:bfvm>")
target_compile_options(bfld PRIVATE -Wall -Wextra -pedantic -g -ggdb)

# Add OS specific sources to target
if (UNIX)
    message(STATUS "Compiling for Linux")
    target_sources(bfld PRIVATE src/linker/linux/ar.c src/linker/linux/elf.c)

else ()
    message(FATAL_ERROR "Platform is not supported")

endif ()
