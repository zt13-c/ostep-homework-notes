# OSTEP Systems Labs：Parallel Zip（pzip）项目笔记

## 1. 项目目标

`pzip` 是 `wzip` 的并行版本。

之前的 `wzip` 是串行 Run-Length Encoding（RLE）压缩：

```text
输入：
aaaaaaaaaabbbbcc

压缩逻辑：
10a 4b 2c
```

但真实输出不是文本 `10a4b2c`，而是二进制格式：

```text
4 字节 int count + 1 字节 char ch
```

例如：

```text
10a
```

实际写出：

```text
0a 00 00 00 61
```

其中：

```text
0a 00 00 00  表示整数 10
61           表示字符 'a'
```

`pzip` 的目标是：

```text
保持 wzip 的输出格式不变
        ↓
但内部使用 pthread 多线程并行压缩
        ↓
最终输出仍然必须和串行 wzip 完全一致
```

---

## 2. 编译方式

```bash
gcc -o pzip pzip.c -Wall -Werror -pthread -O
```

含义：

```text
-Wall      打开常见警告
-Werror    把警告当错误
-pthread   链接 pthread 库，并启用线程相关编译选项
-O         开启优化
```

---

## 3. 基础 RLE 压缩逻辑

RLE 的核心思想是统计连续相同字符。

例如：

```text
aaaaabbbcc
```

扫描过程：

```text
连续 5 个 a → 5a
连续 3 个 b → 3b
连续 2 个 c → 2c
```

代码核心：

```c
if (curr == prev) {
    count++;
} else {
    输出 count 和 prev;
    prev = curr;
    count = 1;
}
```

最后循环结束后，还要输出最后一个 run。

---

## 4. 为什么 pzip 要用 mmap

串行 `wzip` 可以用：

```c
fgetc(fp)
```

逐字符读取文件。

但是并行压缩时，我们希望把文件切成多个 chunk：

```text
文件内容：
[ chunk 0 ][ chunk 1 ][ chunk 2 ][ chunk 3 ]
```

每个线程处理其中一段。

使用 `mmap()` 后，文件会被映射到进程虚拟地址空间中：

```text
磁盘文件
   ↓ mmap
进程虚拟地址空间中的一段内存
   ↓
data[0], data[1], data[2], ...
```

这样每个线程只需要知道：

```c
char *data;
size_t size;
```

就可以压缩自己的那一段。

---

## 5. mmap 处理流程

单个文件的处理流程：

```text
open 打开文件
    ↓
fstat 获取文件大小
    ↓
mmap 映射文件到内存
    ↓
按 chunk 切分
    ↓
线程压缩 chunk
    ↓
munmap 解除映射
    ↓
close 关闭文件描述符
```

典型代码结构：

```c
int fd = open(filename, O_RDONLY);

struct stat sb;
fstat(fd, &sb);

char *data = mmap(NULL,
                  sb.st_size,
                  PROT_READ,
                  MAP_PRIVATE,
                  fd,
                  0);
```

参数含义：

```text
NULL         让操作系统选择映射地址
sb.st_size   映射文件大小
PROT_READ    映射区域可读
MAP_PRIVATE  私有映射
fd           文件描述符
0            从文件偏移 0 开始映射
```

---

## 6. 为什么不能让线程直接输出

错误想法：

```text
线程 0 压缩完 chunk 0，直接 fwrite
线程 1 压缩完 chunk 1，直接 fwrite
线程 2 压缩完 chunk 2，直接 fwrite
```

问题是线程完成顺序不可控。

原始 chunk 顺序是：

```text
chunk 0 → chunk 1 → chunk 2
```

但线程可能完成为：

```text
chunk 2 先完成
chunk 0 后完成
chunk 1 最后完成
```

如果谁先完成谁输出，压缩文件顺序会乱。

正确做法：

```text
worker 线程：
    只负责压缩自己的 chunk
    把结果保存到 result 中

main 主线程：
    等所有 worker 完成
    按 chunk 原始顺序合并输出
```

---

## 7. pzip 的并行结构总览

```text
main thread
    │
    │ 1. open + fstat + mmap
    │
    ▼
+-------------------------------+
| mapped file data              |
+-------------------------------+
    │
    │ 2. split into chunks
    ▼
+---------+---------+---------+---------+
| chunk 0 | chunk 1 | chunk 2 | chunk 3 |
+---------+---------+---------+---------+
    │
    │ 3. create tasks
    ▼
+---------+---------+---------+---------+
| task 0  | task 1  | task 2  | task 3  |
+---------+---------+---------+---------+
    │
    │ 4. worker threads fetch tasks
    ▼
+-----------+      +-----------+      +-----------+
| worker 0  |      | worker 1  |      | worker 2  |
+-----------+      +-----------+      +-----------+
     │                  │                  │
     ▼                  ▼                  ▼
 compress chunk     compress chunk     compress chunk
     │                  │                  │
     ▼                  ▼                  ▼
 task[i].result     task[j].result     task[k].result

    │
    │ 5. main joins workers
    ▼

main thread 按顺序输出：

task[0].result
task[1].result
task[2].result
task[3].result
```

核心思想：

```text
并行的是局部压缩
串行的是全局有序合并
```

---

## 8. chunk_result 结构

每个 chunk 压缩后会产生多个 run。

例如：

```text
chunk 内容：
aaaabbccccc

压缩结果：
4a 2b 5c
```

所以需要结构保存压缩结果：

```c
typedef struct run {
    int count;
    char ch;
} run_t;

typedef struct chunk_result {
    run_t *runs;
    int nruns;
    int capacity;
} chunk_result_t;
```

含义：

```text
runs      保存 run 的动态数组
nruns     当前已有多少个 run
capacity  当前数组容量
```

---

## 9. chunk 边界合并问题

并行压缩最大的难点是 chunk 边界。

例如文件内容：

```text
aaaaaaaaaabbbb
```

如果 `CHUNK_SIZE = 5`，切成：

```text
chunk 0: aaaaa
chunk 1: aaaaabbbb
```

各自压缩：

```text
chunk 0: 5a
chunk 1: 5a 4b
```

如果直接输出：

```text
5a 5a 4b
```

这是错误的。

正确结果应该是：

```text
10a 4b
```

所以主线程输出时不能简单地把每个 chunk 的结果直接写出，而要维护一个全局尚未写出的 run：

```text
global_count
global_ch
```

合并逻辑：

```text
如果当前 run 的字符 == global_ch
    global_count += 当前 run 的 count

否则
    写出 global_count + global_ch
    global_count = 当前 run 的 count
    global_ch = 当前 run 的字符
```

---

## 10. 多文件边界也要合并

`pzip` 支持多个输入文件：

```bash
./pzip a.txt b.txt > out.z
```

逻辑上多个文件应该被看成一个连续输入流。

例如：

```bash
echo -n "aaa" > a.txt
echo -n "aaabbb" > b.txt
```

整体输入等价于：

```text
aaaaaabbb
```

正确压缩结果：

```text
6a 3b
```

而不是：

```text
3a 3a 3b
```

所以 `global_count/global_ch` 不能在每个文件结束时清空，而应该跨文件保留。

只有所有文件全部处理完后，才输出最后一个 global run。

---

## 11. 线程池版本结构

最初可以“一个 chunk 一个线程”，但这不是高性能做法。

如果文件很大：

```text
1000 个 chunk
```

就会创建：

```text
1000 个线程
```

线程创建和调度开销很大。

更好的方式是线程池：

```text
创建固定数量 worker 线程
        ↓
worker 从共享任务队列中抢任务
        ↓
处理完一个任务后继续抢下一个
        ↓
所有任务处理完后退出
```

任务队列可以用一个共享下标表示：

```c
typedef struct work_queue {
    task_t *tasks;
    size_t num_tasks;
    size_t next_task;
    pthread_mutex_t lock;
} work_queue_t;
```

含义：

```text
tasks      所有 chunk 任务
num_tasks  任务总数
next_task  下一个还没被领取的任务编号
lock       保护 next_task 的互斥锁
```

---

## 12. 为什么 next_task 需要锁

多个 worker 会同时执行：

```c
idx = queue->next_task;
queue->next_task++;
```

如果没有锁，可能出现：

```text
worker 0 看到 next_task = 5
worker 1 也看到 next_task = 5
```

结果两个线程都处理 chunk 5。

所以必须加锁：

```c
pthread_mutex_lock(&queue->lock);

size_t idx = queue->next_task;

if (idx >= queue->num_tasks) {
    pthread_mutex_unlock(&queue->lock);
    break;
}

queue->next_task++;

pthread_mutex_unlock(&queue->lock);
```

临界区只保护“领取任务编号”这一步。

真正压缩 chunk 不需要加锁，因为每个线程处理的是不同的 `tasks[idx]`。

---

## 13. worker 线程逻辑

```text
worker_pool:
    while true:
        加锁
        读取 next_task
        如果任务取完:
            解锁
            退出
        next_task++
        解锁

        压缩 tasks[idx]
        结果写入 tasks[idx].result
```

对应代码结构：

```c
static void *worker_pool(void *arg) {
    work_queue_t *queue = (work_queue_t *)arg;

    while (1) {
        pthread_mutex_lock(&queue->lock);

        size_t idx = queue->next_task;

        if (idx >= queue->num_tasks) {
            pthread_mutex_unlock(&queue->lock);
            break;
        }

        queue->next_task++;

        pthread_mutex_unlock(&queue->lock);

        init_result(&queue->tasks[idx].result);
        compress_chunk(queue->tasks[idx].data,
                       queue->tasks[idx].size,
                       &queue->tasks[idx].result);
    }

    return NULL;
}
```

---

## 14. 主线程逻辑

主线程负责：

```text
1. 打开文件
2. mmap 文件
3. 切 chunk
4. 创建 task 数组
5. 创建 worker 线程
6. join 等待所有线程完成
7. 按 task 原始顺序合并输出
8. 释放资源
```

核心结构：

```text
main:
    for each file:
        mmap file
        split into tasks

        create worker threads
        join worker threads

        for j = 0 to num_chunks - 1:
            output_result_with_merge(tasks[j].result)

        free tasks
        munmap file
```

注意输出阶段必须按 `j = 0, 1, 2, ...` 的顺序。

---

## 15. 自测方法

### 15.1 编译

```bash
gcc -o pzip pzip.c -Wall -Werror -pthread -O
```

### 15.2 普通正确性测试

```bash
echo -n "aaaaaaaaaabbbbcc" > a.txt
./pzip a.txt > a.z
../initial-utilities/wunzip/wunzip a.z
```

预期输出：

```text
aaaaaaaaaabbbbcc
```

### 15.3 多文件边界测试

```bash
echo -n "aaa" > a.txt
echo -n "aaabbb" > b.txt
./pzip a.txt b.txt > ab.z
xxd ab.z
```

预期结果应表示：

```text
6a 3b
```

十六进制中应能看到：

```text
06 00 00 00 61 03 00 00 00 62
```

### 15.4 chunk 边界测试

临时把：

```c
#define CHUNK_SIZE ((size_t)(1024 * 1024))
```

改成：

```c
#define CHUNK_SIZE ((size_t)5)
```

然后测试：

```bash
echo -n "aaaaaaaaaabbbb" > c.txt
./pzip c.txt > c.z
xxd c.z
```

正确结果应表示：

```text
10a 4b
```

也就是应该看到：

```text
0a 00 00 00 61 04 00 00 00 62
```

不应该看到：

```text
05 00 00 00 61 05 00 00 00 61
```

否则说明 chunk 边界没有合并。

测试完记得把 `CHUNK_SIZE` 改回 1MB。

### 15.5 和串行 wzip 对比

```bash
./pzip c.txt > pzip.z
../initial-utilities/wzip/wzip c.txt > wzip.z
diff pzip.z wzip.z
```

如果 `diff` 没有输出，说明二进制压缩结果完全一致。

---

## 16. 官方测试说明

官方 `ostep-projects` 根目录 README 提到部分项目有测试脚本，但不是所有项目都有。

当前官方 `concurrency-pzip` 目录下主要是：

```text
README.md
```

没有看到 `test-pzip.sh`。

可以用下面命令确认：

```bash
find . -name "*pzip*" -o -name "test-pzip.sh"
```

所以 `pzip` 通常需要自己写测试，核心判断方式是：

```text
pzip 输出能否被 wunzip 解压回原文
pzip 输出是否和串行 wzip 的输出完全一致
```

---

## 17. 常见错误

### 错误 1：线程直接输出

错误原因：

```text
线程完成顺序不等于 chunk 原始顺序。
```

后果：

```text
压缩结果乱序。
```

正确做法：

```text
线程只压缩，主线程按顺序输出。
```

---

### 错误 2：chunk 边界没有合并

错误结果：

```text
5a 5a 4b
```

正确结果：

```text
10a 4b
```

解决方式：

```text
输出阶段维护 global_count/global_ch。
```

---

### 错误 3：多个文件之间重置 global 状态

错误结果：

```text
a.txt = aaa
b.txt = aaabbb

输出：
3a 3a 3b
```

正确结果：

```text
6a 3b
```

解决方式：

```text
global_count/global_ch 跨文件保留。
```

---

### 错误 4：错误信息写到 stdout

`pzip` 的 stdout 是二进制压缩输出。

如果错误信息也写到 stdout，会污染 `.z` 文件。

错误写法：

```c
printf("pzip: cannot open file\n");
```

更稳妥写法：

```c
fprintf(stderr, "pzip: cannot open file\n");
```

---

### 错误 5：没有加 `-pthread`

如果编译时没有：

```bash
-pthread
```

可能会出现 `pthread_create` 或 `pthread_join` 链接错误。

---

### 错误 6：任务下标没有加锁

如果多个线程同时修改：

```c
next_task
```

可能导致：

```text
同一个 chunk 被处理两次
某些 chunk 没被处理
```

解决方式：

```c
pthread_mutex_lock(&queue->lock);
...
pthread_mutex_unlock(&queue->lock);
```

---

## 18. 总结

`pzip` 的关键不是 RLE 本身，而是如何把串行压缩拆成并行结构。

核心设计是：

```text
mmap 文件
    ↓
切分 chunk
    ↓
worker 线程并行压缩 chunk
    ↓
结果保存到 tasks[i].result
    ↓
main 线程按 chunk 顺序合并输出
```

最重要的思想：

```text
并行计算，串行归并。
```

也就是：

```text
局部压缩可以并行
全局输出必须有序
边界 run 必须合并
```

---

## 19. 易错点

1. `pzip` 输出是二进制，不能用文本方式判断压缩内容。
2. 错误信息不要写到 stdout，否则会污染压缩文件。
3. chunk 内部压缩可以并行，但 chunk 输出必须按原始顺序。
4. chunk 边界上的相同字符必须合并。
5. 多个输入文件之间也要合并边界 run。
6. `next_task` 是共享变量，必须用 mutex 保护。
7. worker 线程只负责压缩，不负责输出。
8. 主线程必须 `pthread_join` 后再输出。
9. `mmap` 空文件通常不合法，空文件应直接跳过。
10. 测试时要同时测普通文件、跨 chunk、跨文件和与串行 `wzip` 的二进制一致性。