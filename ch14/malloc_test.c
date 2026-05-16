#include <stdlib.h>
int main()
{
    int *p = malloc(100*sizeof(int)); // 分配一个整数大小的内存
    return 0; // 程序结束时没有调用 free 来释放内存，导致内存泄漏
}