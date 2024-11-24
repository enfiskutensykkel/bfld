cmake_minimum_required(VERSION 3.16...3.30)
project(bfld)

add_library(bfi STATIC src/interpreter/interpreter.c src/interpreter/foo.c)

add_executable(bfld src/linker/linker.c src/linker/archive.c)
target_include_directories(bfld PUBLIC include)
target_compile_definitions(bfld PUBLIC DEFAULT_INTERPRETER="$<TARGET_FILE:bfi>" FOO=1)
target_compile_options(bfld PRIVATE -Werror -Wall -Wextra -pedantic)
