#include "cdefs.h"
#include <stddef.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <sys/sysctl.h>
#endif

#ifndef DEFAULT_PAGE_SIZE
#define DEFAULT_PAGE_SIZE 4096
#endif

#ifndef DEFAULT_CACHE_LINE_SIZE
#define DEFAULT_CACHE_LINE_SIZE 64
#endif


size_t get_page_size(void)
{
#if defined(_SC_PAGESIZE)
    return (size_t) sysconf(_SC_PAGESIZE);
#elif defined(_SC_PAGE_SIZE)
    return (size_t) sysconf(_SC_PAGE_SIZE);
#else
    return DEFAULT_PAGE_SIZE;
#endif
}


size_t get_cache_line_size(void)
{
#if defined(_SC_LEVEL1_DCACHE_LINESIZE)
    return (size_t) sysconf(_SC_LEVEL1_DCACHE_LINESIZE);

#elif defined(__APPLE__)
    //size_t line_size = DEFAULT_CACHE_LINE_SIZE;
    size_t line_size = 0;
    size_t length = sizeof(line_size);
    sysctlbyname("hw.cachelinesize", &line_size, &length, NULL, 0);
    return line_size;

#else
#error "aa"
    return DEFAULT_CACHE_LINE_SIZE;
#endif
}
