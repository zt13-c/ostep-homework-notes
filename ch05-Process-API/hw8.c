#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

int main() {
    int fd[2];
    // 1. 在 fork 之前创建管道
    if (pipe(fd) < 0) {
        perror("管道创建失败");
        exit(1);
    }

    // 2. 创建第一个子进程（作为发送方 / Writer）
    int rc1 = fork();
    if (rc1 < 0) {
        perror("fork 1 失败");
        exit(1);
    } 
    else if (rc1 == 0) {
        // ================= 子进程 1 (发送方) =================
        close(fd[0]); // 发送方不需要读，关闭读端
        
        // 核心魔法 1：把标准输出 (1) 重定向到管道的写端 (fd[1])
        dup2(fd[1], STDOUT_FILENO); 
        close(fd[1]); // 复制完毕后，关闭原本的 fd[1] 描述符
        
        // 此时，printf 不会打印到屏幕上，而是直接流进管道里！
        printf("这是来自子进程 1 的机密情报，通过管道传输！\n");
        exit(0);
    }

    // 3. 创建第二个子进程（作为接收方 / Reader）
    int rc2 = fork();
    if (rc2 < 0) {
        perror("fork 2 失败");
        exit(1);
    } 
    else if (rc2 == 0) {
        // ================= 子进程 2 (接收方) =================
        close(fd[1]); // 接收方不需要写，关闭写端
        
        // 核心魔法 2：把标准输入 (0) 重定向到管道的读端 (fd[0])
        dup2(fd[0], STDIN_FILENO);
        close(fd[0]); // 复制完毕后，关闭原本的 fd[0] 描述符
        
        char buffer[256];
        // 此时，fgets 以为自己在读键盘输入，实际上它在吸管道里的数据！
        if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
            // 这个 printf 没有被重定向，所以会正常显示在屏幕上
            printf("子进程 2 成功接收到数据: %s", buffer);
        }
        exit(0);
    }

    // ================= 父进程分支 =================
    // 【极其重要】父进程既不读也不写，必须把管道的两端都关掉！
    // 否则子进程 2 的 read 会因为觉得“还有人（父进程）可能要写”而一直死等（死锁）。
    close(fd[0]);
    close(fd[1]);

    // 等待两个子进程双双结束
    waitpid(rc1, NULL, 0);
    waitpid(rc2, NULL, 0);
    printf("父进程：两个子进程已完成管道通信，圆满结束。\n");

    return 0;
}