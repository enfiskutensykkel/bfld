#set(THREADS_PREFER_PTHREAD_FLAG ON)

add_test_executable(strpool FILES stringpool.c OUTPUT_NAME test_strpool)
target_link_libraries(strpool linkerlib Threads::Threads)

# FIXME: remove these? or require libtsan?
#target_compile_options(strpool PRIVATE -fsanitize=thread -g -O1)
#target_link_options(strpool PRIVATE -fsanitize=thread)
