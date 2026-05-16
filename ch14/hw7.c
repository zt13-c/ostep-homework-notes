#include<stdlib.h>
#include<stdio.h>
int main()
{
    int *data = (int *) malloc(100 * sizeof(int));
    int *funny_ptr = data + 50;
    free(funny_ptr); // 错误：释放了 funny_ptr 而不是 data  
    return 0;
}