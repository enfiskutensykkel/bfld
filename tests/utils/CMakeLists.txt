add_test_executable(test_arena FILES arena.c OUTPUT_NAME test_arena)
target_link_libraries(test_arena libbfld Threads::Threads)

add_test_executable(test_locks FILES locks.c OUTPUT_NAME test_locks)
target_link_libraries(test_locks libbfld Threads::Threads)

add_test_executable(test_xxh FILES xxh.c OUTPUT_NAME test_xxh)
target_link_libraries(test_xxh libbfld)

add_test_executable(test_htable FILES htable.c OUTPUT_NAME test_htable)
target_link_libraries(test_htable libbfld Threads::Threads)
target_compile_options(test_htable PRIVATE $<$<CONFIG:Release>:-O3 -march=native -flto>)
target_link_options(test_htable PRIVATE $<$<CONFIG:Release>:-O3 -march=native -flto>)
