# 第 5 章：进程 API (Process API) 课后作业

## 题目 1：验证 fork() 与变量
**题目要求：** 编写一个调用 `fork()` 的程序。在调用前让主进程访问一个变量并赋值。子进程和父进程分别改变该变量，观察会发生什么？

### 1. 实验代码 (`hw1.c`)
```

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

```

### 2. 运行结果
```
【初始状态】 PID: 7594, x = 100
【子进程启动】 PID: 7595, 刚进来的 x = 100
【子进程修改】 PID: 7595, 修改后的 x = 200
【初始状态】 PID: 7594, x = 100
【父进程恢复】 PID: 7594, 此时的 x = 100
【父进程修改】 PID: 7594, 修改后的 x = 300
```

### 3. 实验分析与思考

```
本次实验完美验证了操作系统的**写时复制 (Copy-on-Write) 机制**。
`fork()` 之后，父子进程的虚拟内存虽然指向同一块物理内存，但一旦子进程尝试修改变量 `x`，就会触发缺页中断，操作系统会为子进程单独分配一块新物理内存。因此，父子进程对变量的修改**互不干扰**。
```

## 题目 2：验证 fork() 与文件描述符

**题目要求：** 编写一个打开文件的程序（使用 `open()` 系统调用），然后调用 `fork()` 创建一个新进程。子进程和父进程都可以访问 `open()` 返回的文件描述符吗？当它们并发（即同时）写入文件时，会发生什么？

### 1. 实验代码 (`hw2.c`)

```
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

```
标准输出/重定向 (hw2_output.txt) 的结果
【准备】父进程 (PID: 32955) 打开了文件，获取到的 fd = 3
子进程 (PID: 32956) 写入完毕！
父进程 (PID: 32955) 写入完毕！
```
```
实际被写入文件 (hw2.output) 的结果
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
## 题目 3：不用 wait() 实现父子进程同步

**题目要求：** 使用 `fork()` 编写程序。子进程打印 “hello”，父进程打印 “goodbye”。确保子进程始终先打印。要求不能在父进程调用 `wait()`。

### 1. 实验代码 (`hw3.c`)

```
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
```

### 2. 运行结果
```
hello
goodbye
```
*(无论运行多少次，必定是 hello 在前)*

### 3. 实验分析与思考

```
本题展示了在失去 `wait()` 这一原生工具后，如何利用 **IPC（进程间通信）机制** 来实现强制的同步。

核心原理是利用了 I/O 操作的**阻塞特性 (Blocking)**：
当父进程执行 `read()` 试图读取空管道时，操作系统内核会将其状态从“运行”切换为“阻塞休眠”，并剥夺其 CPU 使用权，强行转交给子进程运行。一旦子进程执行了 `write()`，内核会立刻产生中断，将父进程唤醒。这种方式比使用 `sleep()` 盲目等待要严谨、高效且可靠得多。
```
## 题目 4：探索 exec() 家族的变体

**题目要求：** 编写一个调用 `fork()` 的程序，然后调用某种形式的 `exec()` 来运行程序 `/bin/ls`。尝试 `exec()` 的所有变体。为什么同样的基本调用会有这么多变种？

### 1. 实验代码 (`hw4.c`)

```
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
        execl("/bin/ls", "ls", "-l", "-h", NULL);

        // 2. execle (l = list, e = env): 传列表，且自定义环境变量
        // execle("/bin/ls", "ls", "-l", "-h", NULL, myenv);

        // 3. execlp (l = list, p = path): 传列表，且自动在 PATH 环境变量中寻找程序
        // execlp("ls", "ls", "-l", "-h", NULL); 

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
```
### 2. 运行结果
```
子进程 (PID: 41875) 准备执行 ls 命令...
total 104K
-rwxrwxrwx 1 chenzhituan chenzhituan 7.5K May 10 22:09 Notes.md
-rwxr-xr-x 1 chenzhituan chenzhituan  16K May 10 19:16 hw1
-rw-r--r-- 1 chenzhituan chenzhituan 1.2K May 10 19:02 hw1.c
-rw-r--r-- 1 chenzhituan chenzhituan  289 May 10 19:16 hw1_output.txt
-rwxr-xr-x 1 chenzhituan chenzhituan  17K May 10 21:45 hw2
-rw-r--r-- 1 chenzhituan chenzhituan 1.7K May 10 21:43 hw2.c
-rw-r--r-- 1 chenzhituan chenzhituan   92 May 10 21:45 hw2_output
-rw-r--r-- 1 chenzhituan chenzhituan  151 May 10 21:45 hw2_output.txt
-rwxr-xr-x 1 chenzhituan chenzhituan  16K May 10 22:08 hw3
-rw-r--r-- 1 chenzhituan chenzhituan  991 May 10 22:08 hw3.c
-rwxr-xr-x 1 chenzhituan chenzhituan  16K May 10 22:34 hw4
-rw-r--r-- 1 chenzhituan chenzhituan 2.1K May 10 22:34 hw4.c
父进程 (PID: 41874) 看到子进程执行完毕。

```

### 3. 实验分析与思考

```
在 Linux 操作系统底层，真正的系统调用只有 execve() 这一个。
我们在代码里调用的 execl, execvp 等等，其实都是 C 标准库（glibc）提供的封装函数（Wrapper Functions）。

之所以搞出这么多变种，纯粹是为了给程序员提供极致的方便（语法糖）。我们可以通过后缀字母来破解它们的命名密码：

l (list, 列表)： 适用于你在写代码时就已经确切知道有哪几个参数的情况。你可以直接把参数像变长参数一样一个个写进去（如 "ls", "-l", NULL）。

v (vector, 数组)： 适用于参数是动态生成的。你可以先把参数组装成一个字符串数组（char *args[]），然后一次性传进去。

p (path, 路径)： 这是一个超级福利。如果不带 p，你必须写死程序的绝对路径（比如 /bin/ls）。带了 p，你只需要写程序名（"ls"），C 库会自动去系统的 PATH 环境变量里帮你把真正的可执行文件找出来。

e (environment, 环境变量)： 默认情况下，新程序会继承老程序的所有环境变量。如果你想给新程序一个纯净的、或者定制的运行环境，加上 e 就可以自己传入一个环境变量数组。

总结： 各种组合的出现，是为了应对不同的编程场景。想要图省事就用带 p 的，参数已知就用 l，参数动态组装就用 v。万变不离其宗，它们最终在内核里都会殊途同归，变成对 execve() 的调用。
```

## 题目 5：深入探究 wait() 系统调用

**题目要求：** 编写一个程序，在父进程中使用 `wait()` 等待子进程完成。`wait()` 返回什么？如果你在子进程中使用 `wait()` 会发生什么？

### 1. 实验代码 (`hw5.c`)

```
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
```

### 2. 运行结果
```
父进程 (PID: 40121) 准备调用 wait() 等待子进程 (PID: 40122)...
子进程 (PID: 40122) 开始运行。
【重点】子进程调用 wait() 的返回值是: -1
子进程结束。
【重点】父进程调用 wait() 的返回值是: 40122
父进程结束。
```
### 3. 实验分析与思考
```
父进程中的 wait() 返回什么？

返回终止的子进程的 PID。 在上面的结果中，父进程调用 wait() 的返回值正是子进程的 PID (40122)。

为什么这样设计？ 想象一下如果一个父进程 fork() 了十几个子进程。当它调用 wait() 醒来时，它必须知道到底是哪一个子进程死掉了，返回值就是操作系统给父进程的“死亡通知单上的死者名字”。

在子进程中使用 wait() 会发生什么？

立刻返回 -1。 * 原因分析： wait() 的核心逻辑是“等待任何一个子进程结束”。但在这个程序中，子进程自己并没有调用过 fork()，它是一个“丁克进程”，根本没有孩子。操作系统内核一检查，发现它没有任何正在运行或僵尸状态的子进程，知道它就算等一万年也等不到结果，于是当场报错退回，并返回 -1 代表执行失败。(在标准的 C 库中，此时还会把全局错误码变量 errno 设置为 ECHILD，意为没有子进程)。
```

## 题目 6：使用 waitpid() 替代 wait()

**题目要求：** 对前一个程序稍作修改，这次使用 `waitpid()` 而不是 `wait()`。什么时候 `waitpid()` 会有用？

### 1. 实验代码 (`hw6.c`)

```
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
```

### 2. 运行结果
```
父进程 (PID: 20745) 准备调用 waitpid() 专门等待子进程 (PID: 20746)...
子进程 (PID: 20746) 正在执行任务...
子进程结束。
【重点】父进程 waitpid() 返回，捕获到的子进程 PID 是: 20746
父进程结束。
```
### 3. 实验分析与思考
```
相比于无脑阻塞的 wait()，waitpid() 提供了极大的灵活性，它在以下三种复杂场景中是不可或缺的：

1、等待特定的子进程（最常见应用）：
当一个父进程创建了多个（比如 5 个）子进程去并发执行不同任务时。如果父进程必须等待“老三”执行完才能继续下一步，使用 wait() 是做不到的（因为它等到老大或者老二死掉就会立刻返回）。此时必须使用 waitpid(pid3, ...) 来精准狙击。

2、非阻塞等待（通过 WNOHANG 选项）：
wait() 一旦调用，父进程就会彻底卡死（阻塞）。但 waitpid() 提供了第三个选项参数，如果你传入 WNOHANG：waitpid(rc, &status, WNOHANG)。它的效果是：“我去看看孩子死了没，如果没死，我立刻返回 0 继续干我自己的活儿（比如刷新 UI 界面），过会儿再来看看”。这赋予了父进程异步轮询的能力。

3、等待进程组：
waitpid() 的第一个参数不仅仅可以填具体的 PID，还可以填负数（如 -1 代表等任意子进程，等同于 wait()；或者填 <-1 代表等待特定进程组内的所有子进程），这在编写像 Linux Shell 这种复杂的作业控制系统时非常有用。
```