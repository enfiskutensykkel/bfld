
int another_common;

extern int baz();

int bar = 0;


static void lol(void)
{
    bar = baz();
}
