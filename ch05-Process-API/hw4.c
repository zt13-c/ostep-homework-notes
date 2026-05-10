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
        printf("子进程 (PID: %d) 准备执行 ls 命令...\n", getpid());

        // 准备供 v 系列使用的参数数组 (必须以 NULL 结尾)
        char *myargs[] = {"ls", "-l", "-h", NULL};
        // 准备供 e 系列使用的环境变量数组 (必须以 NULL 结尾)
        char *myenv[]  = {"MY_CUSTOM_ENV=123", NULL};

        // 【提示】下面列出了 6 种不同的 exec 变体。
        // 每次测试时，请保留其中一行未注释，将其余行注释掉。

        // 1. execl (l = list): 参数以列表形式逐个传入
        // execl("/bin/ls", "ls", "-l", "-h", NULL);

        // 2. execle (l = list, e = env): 传列表，且自定义环境变量
        // execle("/bin/ls", "ls", "-l", "-h", NULL, myenv);

        // 3. execlp (l = list, p = path): 传列表，且自动在 PATH 环境变量中寻找程序
        execlp("ls", "ls", "-l", "-h", NULL); 

        // 4. execv (v = vector): 参数以数组(向量)形式传入
        // execv("/bin/ls", myargs);

        // 5. execvp (v = vector, p = path): 传数组，且自动寻找路径
        // execvp("ls", myargs);

        // 6. execvpe (v = vector, p = path, e = env): 传数组，自动找路径，且自定义环境变量
        // （注：部分系统上为 execvP，属于非标准扩展，execvpe 是 GNU 扩展）
        // execvpe("ls", myargs, myenv);

        // 如果 exec 执行成功，下面的代码将永远不会被执行到，因为脑子已经被替换了！
        printf("如果你看到了这行字，说明 exec 失败了！\n");
    } 
    else {
        // ================= 父进程分支 =================
        wait(NULL); // 等待子进程完成“夺舍”和执行
        printf("父进程 (PID: %d) 看到子进程执行完毕。\n", getpid());
    }

    return 0;
}