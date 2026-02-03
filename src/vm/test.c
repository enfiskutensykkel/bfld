extern int printf(const char *format, ...);
extern void _exit(int status);


//void _start(void)
//{
//    printf("Linker test: %s\n", "hello");
//    _exit(0);
//}

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i) {
        printf("Argument %i is %s", i, argv[i]);
    }
    return 0;
}
