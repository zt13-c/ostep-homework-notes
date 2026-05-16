#include <stdio.h>
#include <stdlib.h>

int main() {
    // 1. 分配数组
    int *data = (int *) malloc(100 * sizeof(int));
    data[0] = 999; // 存个数据进去
    
    // 2. 释放内存
    free(data);
    
    // 3. 致命操作：释放后尝试打印
    printf("data[0] = %d\n", data[0]); 
    
    return 0;
}