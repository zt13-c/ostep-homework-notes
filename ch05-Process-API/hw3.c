#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int main() {
    // 建立一个管道: fd[0] 用来读，fd[1] 用来写
    int fd[2];
    if (pipe(fd) < 0) {
        perror("管道创建失败");
        exit(1);
    }

    int rc = fork();

    if (rc < 0) {
        perror("fork 失败");
        exit(1);
    } 
    else if (rc == 0) {
        // --- 子进程分支 ---
        printf("hello\n");
        
        // 打印完后，往管道里随便写一点东西，作为"唤醒信号"
        write(fd[1], "x", 1); 
    } 
    else {
        // --- 父进程分支 ---
        char buffer[10];
        
        // 父进程第一件事就是尝试从管道读取数据。
        // 因为此时管道是空的，操作系统会强制把父进程挂起（阻塞），
        // 直到子进程往里面写了数据，父进程才会被唤醒往下走。
        read(fd[0], buffer, 1); 
        
        printf("goodbye\n");
    }

    return 0;
}