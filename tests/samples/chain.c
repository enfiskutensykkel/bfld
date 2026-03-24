extern int common_symbol;

int *ptr_to_common = &common_symbol;

int foobar()
{
    return *ptr_to_common;
}
