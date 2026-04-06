add_test_executable(test_arena FILES arena.c OUTPUT_NAME test_arena)
target_link_libraries(test_arena libbfld Threads::Threads)
set_target_properties(test_arena PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS ON)

add_test_executable(test_locks FILES locks.c OUTPUT_NAME test_locks)
target_link_libraries(test_locks libbfld Threads::Threads)
set_target_properties(test_locks PROPERTIES C_STANDARD_REQUIRED ON C_EXTENSIONS ON)

add_test_executable(test_xxh FILES xxh.c OUTPUT_NAME test_xxh)
target_link_libraries(test_xxh libbfld)

add_test_executable(test_htable FILES htable.c OUTPUT_NAME test_htable)
target_link_libraries(test_htable libbfld Threads::Threads)
target_compile_options(test_htable PRIVATE $<$<CONFIG:Release>:-O3 -march=native -flto>)
target_link_options(test_htable PRIVATE $<$<CONFIG:Release>:-O3 -march=native -flto>)
set_target_properties(test_htable PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS ON)

find_package(TBB QUIET)

if (TBB_FOUND)
    add_test_executable(test_htable_comparison FILES htable_comparison.cpp OUTPUT_NAME test_htable_comparison)
    target_link_libraries(test_htable_comparison libbfld TBB::tbb Threads::Threads)
    target_compile_options(test_htable_comparison PRIVATE -O3 -march=native)
    target_link_options(test_htable_comparison PRIVATE -O3 -march=native)
    set_target_properties(test_htable_comparison PROPERTIES C_STANDARD 11 CXX_STANDARD 11 C_STANDARD_REQUIRED ON CXX_STANDARD_REQUIRED ON C_EXTENSIONS ON CXX_EXTENSIONS ON)
endif ()
