#include <hash.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

int main(int argc, char **argv)
{
    assert(hash_xxh_32("", 0) == 0x02cc5d05U);
    assert(hash_xxh_32("test", 4) == 0x3e2023cfU);
    assert(hash_xxh_32("Hello World", 11) == 0xb1fd16eeU);
    assert(hash_xxh_32("A very long string that spans more than thirty-two bytes to test the main loop", 78) == 0xc62bdfefU);
    assert(hash_xxh_32("0123456789ABCDEF0123456789ABCDEF", 32) == 0x56317b3bU);

    assert(hash_xxh_64("", 0) == 0xef46db3751d8e999ULL);
    assert(hash_xxh_64("test", 4) == 0x4fdcca5ddb678139ULL);
    assert(hash_xxh_64("Hello World", 11) == 0x6334d20719245bc2ULL);
    assert(hash_xxh_64("A very long string that spans more than thirty-two bytes to test the main loop", 78) == 0x265bdba45dc01198ULL);
    assert(hash_xxh_64("0123456789ABCDEF0123456789ABCDEF", 32) == 0x51acef020cd423b1ULL);

    for (int i = 1; i < argc; ++i) {
        size_t n = strlen(argv[i]);
        uint32_t xxh32 = hash_xxh_32(argv[i], n);
        uint64_t xxh64 = hash_xxh_64(argv[i], n);
        fprintf(stderr, "\nString: '%s'\nXXH32: 0x%08x\nXXH64: 0x%016llx\n", 
                argv[i], xxh32, xxh64);
    }
    return 0;
}
