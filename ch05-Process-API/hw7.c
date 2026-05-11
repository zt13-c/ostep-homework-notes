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
        printf("子进程：关闭 STDOUT 之前，你能看到这句话。\n");
        
        // 关键操作：关闭标准输出（文件描述符 1）
        close(STDOUT_FILENO); 
        
        // 尝试在关闭后打印
        int ret = printf("子进程：关闭 STDOUT 之后，你还能看到这句话吗？\n");
        
        // 我们甚至可以强行把错误信息写到“标准错误 (STDERR)”里来看看 printf 的返回值
        fprintf(stderr, "【调试信息】子进程 printf 返回值: %d\n", ret);
    } 
    else {
        // ================= 父进程分支 =================
        wait(NULL);
        printf("父进程：子进程执行完毕。\n");
    }

    return 0;
}