
if (BUILD_TESTING)
    add_executable(test_rbtree rbtree/main.c)
    target_link_libraries(test_rbtree PRIVATE bfldutils)
    target_include_directories(test_rbtree PRIVATE include/utils)
    add_test(NAME test_bftree COMMAND test_rbtree)
endif()
