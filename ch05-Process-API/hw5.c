#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    int rc = fork();

    if (rc < 0) {
        perror("fork 失败");
        exit(1);
    } 
    else if (rc == 0) {
        // ================= 子进程分支 =================
        printf("子进程 (PID: %d) 开始运行。\n", getpid());
        
        // 尝试在子进程中调用 wait()
        int wait_rc = wait(NULL);
        printf("【重点】子进程调用 wait() 的返回值是: %d\n", wait_rc);
        
        printf("子进程结束。\n");
    } 
    else {
        // ================= 父进程分支 =================
        printf("父进程 (PID: %d) 准备调用 wait() 等待子进程 (PID: %d)...\n", getpid(), rc);
        
        // 捕获 wait() 的返回值
        int wait_rc = wait(NULL);
        printf("【重点】父进程调用 wait() 的返回值是: %d\n", wait_rc);
        
        printf("父进程结束。\n");
    }

    return 0;
}