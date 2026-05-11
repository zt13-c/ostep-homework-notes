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
        printf("子进程 (PID: %d) 正在执行任务...\n", getpid());
        sleep(1); // 模拟耗时任务
        printf("子进程结束。\n");
    } 
    else {
        // ================= 父进程分支 =================
        int status;
        printf("父进程 (PID: %d) 准备调用 waitpid() 专门等待子进程 (PID: %d)...\n", getpid(), rc);
        
        // 使用 waitpid 等待特定的子进程 rc
        // 参数 1: rc (指定要等待的子进程 PID)
        // 参数 2: &status (用于接收子进程退出的状态信息，不需要可以传 NULL)
        // 参数 3: 0 (默认行为，阻塞等待)
        int wait_rc = waitpid(rc, &status, 0);
        
        printf("【重点】父进程 waitpid() 返回，捕获到的子进程 PID 是: %d\n", wait_rc);
        printf("父进程结束。\n");
    }

    return 0;
}