# 第 5 章：进程 API (Process API) 课后作业

## 题目 1：验证 fork() 与变量
**题目要求：** 编写一个调用 `fork()` 的程序。在调用前让主进程访问一个变量并赋值。子进程和父进程分别改变该变量，观察会发生什么？

### 1. 实验代码 (`hw1.c`)
\```c
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
\```

### 2. 运行结果
\```text
【初始状态】 PID: 7594, x = 100
【子进程启动】 PID: 7595, 刚进来的 x = 100
【子进程修改】 PID: 7595, 修改后的 x = 200
【初始状态】 PID: 7594, x = 100
【父进程恢复】 PID: 7594, 此时的 x = 100
【父进程修改】 PID: 7594, 修改后的 x = 300
\```

### 3. 实验分析与思考
本次实验完美验证了操作系统的**写时复制 (Copy-on-Write) 机制**。
`fork()` 之后，父子进程的虚拟内存虽然指向同一块物理内存，但一旦子进程尝试修改变量 `x`，就会触发缺页中断，操作系统会为子进程单独分配一块新物理内存。因此，父子进程对变量的修改**互不干扰**。