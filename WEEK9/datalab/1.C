#include <stdio.h>
int main()
{
    int x = 0x80000000;
    x = ((x >> 31) & ~x | (~!(x >> 31) + 1) & x);
    printf("%x", 1|1);
}