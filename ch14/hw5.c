#include <stdlib.h>

int main() {
    // 申请 100 个整数的空间
    int *data = (int *) malloc(100 * sizeof(int));
    
    // 致命越界！
    data[100] = 0; 
    
    free(data);
    return 0;
}