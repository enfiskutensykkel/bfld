
extern int bar;

int something_in_common;

int initd = 3;

int foo(void)
{
    something_in_common = 67;
    return bar;
}


int baz()
{
    return initd;
}
