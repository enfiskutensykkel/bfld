add_test_executable(test_rbtree FILES rbtree.c OUTPUT_NAME test_rbtree)
target_link_libraries(test_rbtree utilslib)

add_test_executable(test_deque FILES deque.c OUTPUT_NAME test_deque)
target_link_libraries(test_deque utilslib)

add_test_executable(test_arena FILES arena.c OUTPUT_NAME test_arena)
target_link_libraries(test_arena utilslib Threads::Threads)

add_test_executable(test_htable_basics FILES htable_basics.c OUTPUT_NAME test_htable_basics)
target_link_libraries(test_htable_basics utilslib)

add_test_executable(test_locks FILES locks.c OUTPUT_NAME test_locks)
target_link_libraries(test_locks utilslib Threads::Threads)
