add_test_executable(rbtree FILES rbtree.c OUTPUT_NAME test_rbtree)
target_link_libraries(rbtree utilslib)

add_test_executable(deque FILES deque.c OUTPUT_NAME test_deque)
target_link_libraries(deque utilslib)

add_test_executable(stringpool FILES stringpool.c OUTPUT_NAME test_stringpool)
target_link_libraries(stringpool utilslib)
