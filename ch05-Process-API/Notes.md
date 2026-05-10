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

## 题目 2：验证 fork() 与文件描述符

**题目要求：** 编写一个打开文件的程序（使用 `open()` 系统调用），然后调用 `fork()` 创建一个新进程。子进程和父进程都可以访问 `open()` 返回的文件描述符吗？当它们并发（即同时）写入文件时，会发生什么？

### 1. 实验代码 (`hw2.c`)

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>

int main() {
    // 1. 在 fork 之前，打开或创建一个文件
    int fd = open("hw2.output", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        perror("文件打开失败");
        exit(1);
    }

    printf("【准备】父进程 (PID: %d) 打开了文件，获取到的 fd = %d\n", getpid(), fd);
    
    // 关键修复：强制刷新缓冲区，防止 fork() 复制父进程未排空的缓冲区内容
    fflush(stdout); 

    // 2. 调用 fork()
    int rc = fork();

    if (rc < 0) {
        perror("fork 失败");
        exit(1);
    } 
    else if (rc == 0) {
        // --- 子进程分支 ---
        const char *child_msg = "这是来自【子进程】的一行文字。\n";
        write(fd, child_msg, strlen(child_msg));  // 系统调用，无缓冲直接写入文件
        printf("子进程 (PID: %d) 写入完毕！\n", getpid());
    } 
    else {
        // --- 父进程分支 ---
        const char *parent_msg = "这是来自【父进程】的一行文字。\n";
        write(fd, parent_msg, strlen(parent_msg));
        printf("父进程 (PID: %d) 写入完毕！\n", getpid());
        wait(NULL); // 等待子进程结束
    }

    // 3. 各自关闭文件描述符
    close(fd);
    return 0;
}
```

### 2. 运行结果

```标准输出/重定向 (hw2_output.txt) 的结果
【准备】父进程 (PID: 32955) 打开了文件，获取到的 fd = 3
子进程 (PID: 32956) 写入完毕！
父进程 (PID: 32955) 写入完毕！
```
```实际被写入文件 (hw2.output) 的结果
这是来自【父进程】的一行文字。
这是来自【子进程】的一行文字。
注：两行文字的先后顺序可能会因为操作系统的进程调度而改变
```

### 3. 实验分析与思考

```
本实验揭示了 fork() 机制下的文件共享状态与 I/O 缓冲机制：

父子进程共享文件描述符吗？ 是的。 fork() 发生时，操作系统不仅拷贝了进程的内存空间，还拷贝了文件描述符表。因此，父进程获取的 fd = 3 会被子进程完美继承，它们都指向硬盘上的同一个文件。

并发写入会发生什么？
它们的内容会被依次追加到文件中，绝不会互相覆盖。这是因为在 OS 内核中，这两个拷贝的 fd 共享着同一个文件偏移量（指针）。然而，谁先写入完全取决于操作系统的 CPU 调度策略，这种结果的不可预测性是典型的“竞争条件（Race Condition）”。

额外踩坑与收获：C语言缓冲区陷阱
实验中发现，如果不加 fflush(stdout); 且将输出重定向到文件时，父进程的 printf 准备语句会被异常打印两次。这是由于 printf 在重定向时开启了全缓冲，fork() 会把装满数据但尚未吐出的内存缓冲区一并复制给子进程。最终子进程和父进程结束时，各自倒空了一次一模一样的缓冲区。而 write() 是无缓冲的系统调用，直达内核，所以不会受到此陷阱影响。
```
