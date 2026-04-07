cmake_minimum_required(VERSION 3.16...3.30)
project(bfld)

set(THREADS_PERFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
include(CheckIncludeFile)
include(CheckCSourceCompiles)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_C_STANDARD_REQUIRED ON)


# Atomics is required for bfld to work
check_c_source_compiles("
#include <stdatomic.h>
int main() { _Atomic int i = 0; return 0; }
" HAVE_C11_ATOMICS)
if (HAVE_C11_ATOMICS)
    add_compile_definitions(HAS_C11_ATOMICS)
    message(STATUS "Compiler supports C11 atomic operations")
endif ()

check_c_source_compiles("
int main() { volatile int *p = 0; __atomic_store_n(&p, 42, __ATOMIC_SEQ_CST); return 0; }
" HAVE_BUILTIN_ATOMICS)
if (HAVE_BUILTIN_ATOMICS)
    add_compile_definitions(HAS_BUILTIN_ATOMICS)
    message(STATUS "Compiler supports built-in atomic operations")
endif ()

if (NOT HAVE_BUILTIN_ATOMICS AND NOT HAVE_C11_ATOMICS)
    message(FATAL_ERROR "Compiler does not support C11 atomic operations")
endif ()

# Do some feature tests
check_c_source_compiles("
int main() { __asm__ __volatile__ (\"pause\"); return 0; }
" HAVE_INLINE_ASM_PAUSE)
if (HAVE_INLINE_ASM_PAUSE)
    add_compile_definitions(HAS_ASM_PAUSE)
endif ()

check_c_source_compiles("
int main() { __asm__ __volatile__ (\"yield\"); return 0; }
" HAVE_INLINE_ASM_YIELD)
if (HAVE_INLINE_ASM_YIELD)
    add_compile_definitions(HAS_ASM_YIELD)
endif ()

check_c_source_compiles("
#include <immintrin.h>
int main() { _mm_pause(); return 0; }
" HAVE_INTEL_INTRINSICS_PAUSE)
if (HAVE_INTEL_INTRINSICS_PAUSE)
    add_compile_definitions(HAS_INTEL_INTRINSICS_PAUSE)
endif ()

check_include_file("valgrind/memcheck.h" HAVE_VALGRIND_MEMCHECK_H)
if (HAVE_VALGRIND_MEMCHECK_H)
    add_compile_definitions(HAS_VALGRIND)
    message(STATUS "Detected valgrind memcheck header")
endif ()

check_c_source_compiles("
#include <threads.h>
int main() { thrd_yield(); return 0; }
" HAVE_C11_THREADS)
if (HAVE_C11_THREADS)
    add_compile_definitions(HAS_C11_THREADS)
endif ()


check_c_source_compiles("
#include <stdalign.h>
struct test { alignas(64) int value; };
int main() { return 0; }
" HAVE_C23_ALIGNAS)
if (HAVE_C23_ALIGNAS)
    add_compile_definitions(HAS_C23_ALIGNAS)
endif ()

check_c_source_compiles("
#include <stdlib.h>
#include <sys/mman.h>
int main() { void *p = NULL; madvise(p, 64, MADV_HUGEPAGE); return 0; }
" HAVE_MADVISE)
if (HAVE_MADVISE)
    add_compile_definitions(HAS_MADVISE)
endif ()

check_c_source_compiles("
#include <stdlib.h>
int main() { aligned_alloc(64, 64); return 0; }
" HAVE_ALIGNED_ALLOC)
if (HAVE_ALIGNED_ALLOC)
    add_compile_definitions(HAS_ALIGNED_ALLOC)
endif ()

check_c_source_compiles("
#include <stdlib.h>
int main() { void *p = NULL; posix_memalign(&p, 64, 64); return 0; }
" HAVE_POSIX_MEMALIGN)
if (HAVE_POSIX_MEMALIGN)
    add_compile_definitions(HAS_POSIX_MEMALIGN)
endif ()

check_c_source_compiles("
#include <unistd.h>
#include <sched.h>
int main() { sched_yield(); return 0; }
" HAVE_SCHED_YIELD)
if (HAVE_SCHED_YIELD)
    add_compile_definitions(HAS_SCHED_YIELD)
endif ()


# Add compile and linker definitions for TSan profiling
add_compile_options("$<$<CONFIG:TSan>:-fsanitize=thread;-g;-O1>")
add_link_options("$<$<CONFIG:TSan>:-fsanitize=thread>")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Set build type" FORCE)
endif()

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
add_library(libbfld SHARED
     src/arena.c
     src/htable.c
     src/mfile.c
     src/linker.c
     src/sysconf.c
     src/ftruncate.c
)
target_include_directories(libbfld PUBLIC src) 
target_compile_options(libbfld PRIVATE -Wall -Wextra -pedantic)
set_target_properties(libbfld PROPERTIES OUTPUT_NAME bfld)


# Make an executable linker
add_executable(bfld
    src/arena.c
    src/htable.c
    src/mfile.c
    src/linker.c
    src/main.c
    src/sysconf.c
    src/ftruncate.c
)
target_include_directories(bfld PRIVATE src)
target_compile_options(bfld PRIVATE -Wall -Wextra -pedantic)
set_target_properties(bfld PROPERTIES OUTPUT_NAME bfld)

# TODO: at some point we want to register loaders/frontends with dlsym/dlopen and determine loaders/frontends with command line arguments (then point default loader based on architecture using if (macos/unix)

#add_subdirectory(src/frontends)
#add_subdirectory(src/targets)

