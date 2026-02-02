extern int printf(const char *format, ...);
extern void _exit(int status);


void _start(void)
{
    printf("Linker test: %s\n", "hello");
    _exit(0);
}
