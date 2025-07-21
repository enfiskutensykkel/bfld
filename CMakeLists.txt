cmake_minimum_required(VERSION 3.16...3.30)
project(bfld)

add_library(bfvm STATIC src/vm/bfvm.c src/vm/foo.c)

add_executable(bfld src/linker/linker.c src/linker/archive.c)
target_include_directories(bfld PUBLIC include)
target_compile_definitions(bfld PUBLIC DEFAULT_INTERPRETER="$<TARGET_FILE:bfvm>")
target_compile_options(bfld PRIVATE -Wall -Wextra -pedantic -g -ggdb)
