#add_test_executable(test_rbtree FILES rbtree.c OUTPUT_NAME test_rbtree)
#target_link_libraries(test_rbtree utilslib)
#
#add_test_executable(test_deque FILES deque.c OUTPUT_NAME test_deque)
#target_link_libraries(test_deque utilslib)
#
#add_test_executable(test_rht_basics FILES rht_basics.c OUTPUT_NAME test_rht_basics)
#target_link_libraries(test_rht_basics utilslib)

add_test_executable(test_arena FILES arena.c OUTPUT_NAME test_arena)
target_link_libraries(test_arena utilslib Threads::Threads)

add_test_executable(test_locks FILES locks.c OUTPUT_NAME test_locks)
target_link_libraries(test_locks utilslib Threads::Threads)

add_test_executable(test_xxh FILES xxh.c OUTPUT_NAME test_xxh)
target_link_libraries(test_xxh utilslib)

add_test_executable(test_htable FILES htable.c OUTPUT_NAME test_htable)
target_link_libraries(test_htable utilslib Threads::Threads)
