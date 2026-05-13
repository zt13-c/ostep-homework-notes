#define _GNU_SOURCE // 必须定义，为了使用 sched_setaffinity
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sched.h>
#include <sys/wait.h>

// 获取当前时间的微秒级时间戳
long long get_time_us() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000000LL) + tv.tv_usec;
}

// 将当前进程绑定到 CPU 核心 0
void bind_to_cpu0() {
    cpu_set_t set;
    CPU_ZERO(&set);        // 清空集合
    CPU_SET(0, &set);      // 将 CPU 0 加入集合
    // 设置当前进程(0代表当前进程)的 CPU 亲和性
    if (sched_setaffinity(0, sizeof(cpu_set_t), &set) == -1) {
        perror("sched_setaffinity failed");
        exit(1);
    }
}

int main() {
    long long start, end;
    int iterations = 1000000; // 迭代次数（100万次，为了让 gettimeofday 足够精确）

    printf("========== OS 性能测量实验 ==========\n");

    // =================================================================
    // 任务 1：测量单次系统调用的成本
    // 策略：调用 read(0, NULL, 0) 100万次，计算平均时间。
    // =================================================================
    start = get_time_us();
    for (int i = 0; i < iterations; i++) {
        // 读取 0 字节，这是一个合法的空操作系统调用，能纯粹触发 trap 陷入内核
        read(0, NULL, 0); 
    }
    end = get_time_us();

    double total_syscall_time = (double)(end - start);
    double avg_syscall_time = total_syscall_time / iterations;
    printf("[1] 平均系统调用时间: %f 微秒 (共执行 %d 次)\n", avg_syscall_time, iterations);


    // =================================================================
    // 任务 2：测量上下文切换的成本
    // 策略：创建两个管道，父子进程互相打乒乓球。绑定到同一个 CPU。
    // =================================================================
    int pipe1[2]; // 父写 -> 子读
    int pipe2[2]; // 子写 -> 父读

    if (pipe(pipe1) == -1 || pipe(pipe2) == -1) {
        perror("pipe creation failed");
        exit(1);
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
        exit(1);
    } 
    else if (pid == 0) {
        // ---------------- 子进程 ----------------
        bind_to_cpu0(); // 作业要求：绑定到同一个 CPU，防止多核并行干扰
        char msg_child;
        for (int i = 0; i < iterations; i++) {
            read(pipe1[0], &msg_child, 1); // 阻塞，等待父进程写入 -> 触发上下文切换
            write(pipe2[1], "a", 1);       // 写入数据，唤醒父进程
        }
        exit(0); // 子进程完成任务后退出
    } 
    else {
        // ---------------- 父进程 ----------------
        bind_to_cpu0(); // 父进程也绑定到 CPU 0
        char msg_parent;

        start = get_time_us();
        for (int i = 0; i < iterations; i++) {
            write(pipe1[1], "a", 1);       // 写入数据，唤醒子进程
            read(pipe2[0], &msg_parent, 1); // 阻塞，等待子进程写回 -> 触发上下文切换
        }
        end = get_time_us();
        
        wait(NULL); // 等待子进程彻底死亡，清理 PCB

        double total_switch_time = (double)(end - start);
        
        // 核心数学计算：
        // 1 次循环 = 父写 -> 父阻塞切换到子(1次切换) -> 子读 -> 子写 -> 子阻塞切换到父(1次切换) -> 父读
        // 所以 1 次循环经历了 2 次上下文切换。
        // 总切换次数 = iterations * 2。
        double avg_switch_cost = total_switch_time / (iterations * 2);

        printf("[2] 平均上下文切换时间: %f 微秒 (基于 %d 次往返)\n", avg_switch_cost, iterations);
        
        // 进阶推导：严格来说，这里的往返时间还包含了 pipe 的 read 和 write 系统调用的成本。
        // 如果追求极度精确，应该减去系统调用的时间。
        double pure_switch_cost = avg_switch_cost - avg_syscall_time;
        printf("    -> 扣除管道系统调用开销后的纯切换时间 (约): %f 微秒\n", pure_switch_cost);
    }

    return 0;
}