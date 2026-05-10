#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
    // 1. 在调用 fork() 之前，定义变量 x 并赋值为 100
    int x = 100;
    printf("【初始状态】 PID: %d, x = %d\n", getpid(), x);

    // 2. 调用 fork()
    int rc = fork();

    if (rc < 0) {
        // fork 失败
        fprintf(stderr, "fork failed\n");
        exit(1);
    } 
    else if (rc == 0) {
        // ================= 子进程分支 =================
        printf("【子进程启动】 PID: %d, 刚进来的 x = %d\n", getpid(), x);
        
        // 子进程改变 x 的值
        x = 200;
        printf("【子进程修改】 PID: %d, 修改后的 x = %d\n", getpid(), x);
        
    } 
    else {
        // ================= 父进程分支 =================
        // 为了让输出好看点，我们让父进程稍微等一下，让子进程先跑完
        wait(NULL); 
        
        printf("【父进程恢复】 PID: %d, 此时的 x = %d\n", getpid(), x);
        
        // 父进程改变 x 的值
        x = 300;
        printf("【父进程修改】 PID: %d, 修改后的 x = %d\n", getpid(), x);
    }

    return 0;
}