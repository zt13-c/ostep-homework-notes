#include <stdlib.h>
int main()
{
    int * p = NULL; // 定义一个空指针，指向地址 0
    *p = 100; // 试图释放一个空指针，这在 C 标准中是合法的，但在某些系统上可能会导致段错误
    return 0;
}