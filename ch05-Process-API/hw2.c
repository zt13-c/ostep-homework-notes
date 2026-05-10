#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>    // 包含了 open() 函数需要的常数（如 O_CREAT）
#include <string.h>
#include <sys/wait.h>

int main()
{
    // 1. 在 fork() 之前，使用 open() 打开或创建一个文件
    // O_CREAT: 不存在则创建; O_WRONLY: 只写模式; O_TRUNC: 每次运行清空旧内容
    // 0644 是设置文件权限 (rw-r--r--)
    int fd = open("hw2_output",O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if(fd < 0)
    {
        perror("文件打开失败");
        exit(1);
    }
    printf("【准备】父进程 (PID: %d) 打开了文件，获取到的 fd = %d\n", getpid(), fd);

    fflush(stdout);
    
    // 2. 调用 fork()
    int rc = fork();
    if (rc < 0) {
        perror("fork 失败");
        exit(1);
    }
    else if (rc == 0) {
        // ================= 子进程分支 =================
        const char *child_msg = "这是来自【子进程】的一行文字。\n";
        
        // 尝试使用父进程 open() 返回的 fd 写入文件
        write(fd, child_msg, strlen(child_msg));
        printf("子进程 (PID: %d) 写入完毕！\n", getpid());
    }
    else {
        // ================= 父进程分支 =================
        const char *parent_msg = "这是来自【父进程】的一行文字。\n";
        
        // 尝试写入文件
        write(fd, parent_msg, strlen(parent_msg));
        printf("父进程 (PID: %d) 写入完毕！\n", getpid());
        
        // 等待子进程结束
        wait(NULL);
    }
    // 3. 无论是父进程还是子进程，用完文件都要各自 close 自己的描述符
    close(fd);
    return 0;
}