#include <pthread.h>//提供线程函数
#include <stdio.h>// 提供标准I/O函数：fopen, fgetc, fwrite, printf等
#include <stdlib.h>// 提供exit函数
#include <sys/mman.h>// 提供内存映射函数：mmap, munmap等和 MAP_FAILED
#include <sys/stat.h>// 提供文件状态函数：fstat等和 struct stat
#include <fcntl.h>// 提供文件控制函数：open等
#include <unistd.h>// 提供Unix系统服务函数：close等

#define CHUNK_SIZE ((size_t)(1024 * 1024)) // 1MB，定义每次读取的文件块大小

typedef struct run{
    int count;
    char ch;
}run_t;// 定义一个结构体 run_t，用于表示一个游程编码单元，包含连续字符的计数和字符本身。一个run_t实例表示一个压缩后的数据单元，例如 "aaaa" 可以表示为 run_t{count=4, ch='a'}。每个 chunk 要先单独压缩，结果先保存起来，最后再按顺序输出。

typedef struct chunk_result{
    run_t *runs;
    int nruns;
    int capacity;
}chunk_result_t;// 定义一个结构体 chunk_result_t，用于表示一个文件块的压缩结果。它包含一个动态数组 runs，存储该块的所有游程编码单元；nruns 表示当前存储的游程数量；capacity 表示 runs 数组的容量。这个结构体用于在处理每个文件块时保存压缩结果，最后将所有块的结果合并输出。
//为什么runs是一个动态数组？其定义不是一个指向run_t的指针吗？
//答：是的，runs 是一个指向 run_t 的指针，但它被用作动态数组。通过 malloc 分配内存后，runs 指向一块连续的内存区域，这块区域可以存储多个 run_t 实例。通过调整 capacity 和 nruns，可以动态管理这个数组的大小和使用情况。这样设计的好处是可以在运行时根据需要扩展数组，而不是在编译时固定大小，从而更灵活
//这跟字符数组的动态分配类似，例如 char *str = malloc(100) 可以存储 100 个字符，str 是一个指针，但它指向一块连续的内存区域，可以像数组一样使用。在该数组中，一个索引间隔就是一个该指针指向的类型的大小，例如 run_t *runs = malloc(10 * sizeof(run_t)) 可以存储 10 个 run_t 实例，runs[0]、runs[1] 等就像数组一样访问每个 run_t 实例。

typedef struct task{//定义线程任务结构
    char *data;//自己压缩哪一段 data
    size_t size;//这一段有多长 size
    chunk_result_t result;//压缩结果放在哪里 result
}task_t;

typedef struct work_queue{
    task_t *tasks;
    size_t num_tasks;
    size_t next_task;
    pthread_mutex_t lock;
}work_queue_t;

static void usage_error(void){
    fprintf(stderr, "pzip: file1 [file2 ...]\n");
    exit(1);
}

static void file_error(void) {
    fprintf(stderr, "pzip: cannot open file\n");
    exit(1);
}

static void internal_error(const char *msg) {
    fprintf(stderr, "pzip: %s\n", msg);
    exit(1);
}

//初始化chunk_result_t结构体
static void init_chunk_result(chunk_result_t *result) {
    result->capacity = 16; // 初始容量为16个游程编码单元
    result->nruns = 0; // 当前游程数量为0
    result->runs = malloc(result->capacity * sizeof(run_t)); // 分配内存
    if (result->runs == NULL) internal_error("malloc failed");
}

static void free_result(chunk_result_t *res){
    free(res->runs);
    res->runs = NULL;
    res->nruns = 0;
    res->capacity = 0;
}

static void write_run(int count, char ch) {
    fwrite(&count, sizeof(int), 1, stdout);
    fwrite(&ch, sizeof(char), 1, stdout);
}

//添加 run 到 chunk_result
static void append_run(chunk_result_t *res,int count,char ch){
    if(count <= 0) return;
    if(res->nruns > 0 && res->runs[res->nruns-1].ch == ch){
        res->runs[res->nruns-1].count += count;
        return;
    }
    if(res->nruns == res->capacity){
        res->capacity *= 2;
        res->runs = realloc(res->runs,res->capacity * sizeof(run_t));
        //realloc函数： 保持原有数据 + 调整内存大小 + 自动处理旧内存释放
        //参数：ptr：之前由 malloc()、calloc() 或 realloc() 返回的内存指针   new_size：需要的新大小（字节数）   返回值：指向新内存块的指针，失败返回 NULL
        //如果 ptr == NULL → 行为等同于 malloc(new_size);如果 new_size == 0 → 行为等同于 free(ptr)，返回 NULL;如果 ptr 有效且 new_size > 0
        // a. 尝试在原有位置扩展内存（如果后面有足够空间）         
        //     → 成功则返回原指针，数据不变                        
        // b. 如果原地无法扩展                                     
        //     → 找一块新的大内存                                   
        //     → 将原数据拷贝到新内存                               
        //     → 释放原内存                                         
        //     → 返回新指针                   
        if(res->runs == NULL) internal_error("realloc failed");
    }
    res->runs[res->nruns].count = count;
    res->runs[res->nruns].ch = ch;
    res->nruns++;
}

//输出 chunk_result，并处理边界合并
static void output_result_with_merge(chunk_result_t *res,int *global_count,char *global_ch){//这里global_count和global_ch使用指针是因为需要修改函数外部的变量，否则C语言是值传递，函数只能修改副本
    for(int i = 0;i < res->nruns;i++){
        int count = res->runs[i].count;
        char ch = res->runs[i].ch;
        if(*global_count == 0){
            *global_count = count;
            *global_ch = ch;
        }else if(*global_ch == ch){
            *global_count += count;
        }else{
            write_run(*global_count,*global_ch);
            *global_count = count;
            *global_ch = ch;
        }
    }
}

static void compress_chunk(char *data,size_t size,chunk_result_t *res){
    if(size == 0) return;
    char prev = data[0];
    int count = 1;
    for(size_t i = 1;i < size;i++){
        char curr = data[i];
        if(curr == prev) count++;
        else{
            append_run(res,count,prev);
            prev = curr;
            count = 1;
        }
    }
    append_run(res,count,prev);
}

//线程函数worker
static void *worker_pool(void *arg){//线程函数必须是void *func(void *arg)，在 C 语言的 pthread 库中，创建线程的函数设计者面临一个问题：他们不知道你到底想传什么类型的数据给线程（可能是整数、字符串、复杂的结构体等）。
    work_queue_t *queue = (work_queue_t *)arg;
    while(1){
        pthread_mutex_lock(&queue->lock);
        size_t idx = queue->next_task;
        if(idx >= queue->num_tasks){
            pthread_mutex_unlock(&queue->lock);
            break;
        }
        queue->next_task++;
        pthread_mutex_unlock(&queue->lock);

        init_chunk_result(&queue->tasks[idx].result);
        compress_chunk(queue->tasks[idx].data,queue->tasks[idx].size,&queue->tasks[idx].result);
    }
    return NULL; 
}

//获取线程数量函数,希望线程数量接近 CPU 核心数
static int get_thread_count(void){
    long n = sysconf(_SC_NPROCESSORS_ONLN);//得到当前系统可用的 CPU 核心数。
    if(n < 1) return 1;
    return (size_t)n;
}

int main(int argc, char *argv[]) {
    if (argc < 2) usage_error();

    int global_count = 0;
    char global_ch = 0;

    for(int i = 1;i < argc;i++){
        int fd = open(argv[i],O_RDONLY);//使用 open() 而不是 fopen()，因为我们要使用 mmap()，而 mmap() 需要文件描述符。
        if(fd < 0){
            close(fd);
            file_error();
        }
        struct stat st;
        if(fstat(fd,&st) < 0){//使用 fstat() 获取文件大小和其他信息，fstat() 是通过文件描述符获取文件状态的函数。具体参数说明：
            // fd: 文件描述符，指向我们要获取状态的文件。
            // st: 指向 struct stat 结构体的指针，fstat() 会 填充这个结构体，包含文件大小、权限、时间戳等信息。
            // 返回值: 成功返回 0，失败返回 -1 并设置 errno。
            close(fd);
            internal_error("cannot stat file");              
        }

        if(st.st_size == 0){//如果文件大小为 0，直接跳过这个文件，不进行压缩。
            close(fd);
            continue;
        }

        char *data = mmap(NULL,//NULL 表示让内核选择映射的起始地址。
                          st.st_size,//映射的长度，单位是字节，这里是整个文件的大小。
                          PROT_READ,//映射区域的保护标志，这里是只读。
                          MAP_PRIVATE,//映射类型标志，MAP_PRIVATE 表示对映射区域的修改不会影响原文件。
                          fd,//要映射的文件描述符。
                          0);//文件偏移量，表示从文件的开头开始映射。
        if(data == MAP_FAILED){//如果 mmap() 失败，返回 MAP_FAILED（通常是 (void *) -1），表示映射失败。
            close(fd);
            internal_error("cannot mmap file");   
        }

        size_t file_size = (size_t)st.st_size;
        size_t num_chunks = (file_size + CHUNK_SIZE - 1)/CHUNK_SIZE;//向上取整,它确保即使文件大小不能被 CHUNK_SIZE 整除，最后剩余的字节也能单独分到一个数据块中。

        task_t *tasks = malloc(sizeof(task_t) * num_chunks);//为每个分块分配一个任务结构体 (task_t)，用来记录每个线程需要处理的数据。

        if(tasks == NULL) internal_error("malloc failed");

        for(size_t j = 0;j < num_chunks;j++){
            size_t offset = (size_t)(j * CHUNK_SIZE);
            size_t remaining = file_size - offset;
            size_t chunk_size = remaining < CHUNK_SIZE ? remaining : CHUNK_SIZE;

            tasks[j].data = data + offset;
            tasks[j].size = chunk_size;

        }

        size_t num_workers = get_thread_count();

        if(num_workers >= num_chunks) num_workers = num_chunks;

        pthread_t *threads = malloc(sizeof(pthread_t) * num_workers);

        if(threads == NULL){
            free(tasks);
            internal_error("malloc failed");
        }

        work_queue_t queue;
        queue.tasks = tasks;   
        queue.num_tasks = num_chunks;
        queue.next_task = 0;

        pthread_mutex_init(&queue.lock,NULL);

        for(size_t j = 0;j < num_workers;j++){
            if(pthread_create(&threads[j],NULL,worker_pool,&queue) != 0){//在 C 语言中，数组下标操作符 [] 其实是“解引用（Dereference）”的语法糖。threads[j] 等价于 *(threads + j),所以要传入&threads[j],等价于threads+j
                /*
                函数原型：int pthread_create(pthread_t *thread, //这是一个输出参数。你需要事先准备好一个 pthread_t 类型的变量（比如前面代码里的 threads[j]），并把它的地址传进去。当操作系统成功创建线程后，会生成一个唯一的“线程 ID”，并把它写入到这个地址里。后续如果你想等待这个线程（pthread_join）或者强行终止它，都需要凭这个 ID 办事。
                                            const pthread_attr_t *attr,//用于设置新线程的特殊属性，比如栈大小、调度策略、是否分离（detached）等。对于 99% 的基础并发程序，我们不需要这些特殊设置。直接传 NULL，表示使用操作系统的默认设置。
                                            void *(*start_routine) (void *), //这是一个函数指针，告诉新线程“你诞生之后要去执行哪段代码”。这个函数必须严格符合我们上一问提到的格式——接收一个 void * 参数，返回一个 void * 结果。在你的代码中，这里传入的就是 worker 函数的名（在 C 语言中，函数名本身就是函数的地址）。
                                            void *arg);//传递给目标函数（参数 3）的输入数据。因为只能传一个参数，所以通常我们会把需要传的数据打包成一个结构体（比如你代码里的 work_queue_t）。这里传入 &queue 的地址。当新线程启动并调用 worker 函数时，这个地址就会变成 worker 函数里的 void *arg。
                返回 0： 表示线程创建成功，新线程已经开始独立运行。
                返回非 0 值： 表示创建失败。可能是因为系统资源耗尽（比如达到了最大线程数限制），或者内存不足。返回值本身就是一个错误码（如 EAGAIN, EINVAL）。

                worker不是变量，而是“常量标签（Symbol）”。在 C 语言中，变量（比如 int a 或 void *ptr）会在内存中占据实际的空间（比如 4 字节或 8 字节），用来存放随时可以改变的值。但函数名 worker 不同：
                它不能被赋值： 你不能写 worker = &other_function;，因为它不是一个用来装地址的容器。
                它本身就是地址： 就像现实中的“天安门”这个词。它不是一个能装东西的盒子，它直接代表了一个绝对的地理位置。
                在传参时发生了什么？ 在 pthread_create(..., worker, ...) 中，worker 作为一个常量地址值，被传递给了 pthread_create 内部真正的指针变量 start_routine。此时，start_routine 才是在内存（栈）中真实存在的一个指针变量，里面装着 worker 对应的数字地址。

                变量举例：
                1. 编译前的源代码 (人类视角)
                你写下了这行代码：
                int a = 0;
                a = a + 5;
                在你眼里，a 是一个装了数字 0 的盒子。

                2. 编译时的翻译 (编译器视角)
                编译器（比如 GCC）在读取这段代码时，会在自己的内部建立一张临时表格（符号表）：
                编译器心想：“人类需要一个叫 a 的整数。我来看看栈内存里哪里有空位……好，当前的基准地址往下退 4 个字节（假设为 -4 的位置）是空的。”
                它在草稿本上记下：名字 a == 内存地址 [栈底指针 - 4]。
                接下来，编译器开始把你的代码翻译成机器指令：
                看到 int a = 0; ➔ 翻译成机器码：往 [栈底指针 - 4] 这个地址写入数字 0。
                看到 a = a + 5; ➔ 翻译成机器码：把 [栈底指针 - 4] 里的数字读出来，加上 5，再写回 [栈底指针 - 4]。
                翻译完成后，编译器就把那张记着 a 的草稿纸直接撕掉烧了。
                3. 运行时的执行 (CPU 视角)
                当你的程序双击运行，交由 CPU 执行时，CPU 看到的纯粹是底层的汇编指令（机器码）：
                代码段
                MOV DWORD PTR [rbp-4], 0   ; 把 0 塞进 rbp寄存器减4 的内存位置
                ADD DWORD PTR [rbp-4], 5   ; 把那个位置的数加 5
                你看，这里的指令中，还有字符 a 吗？完全没有。CPU 根本不知道这个 [rbp-4] 的位置曾经被人类叫做 a。它只是个无情的搬砖机器，按照指令，在特定的物理内存格子里填入数字而已。
                */
                internal_error("pthread_create failed");
            }
        }

        for(size_t j = 0;j < num_workers;j++){//主线程 join 等全部完成
            pthread_join(threads[j],NULL);
        }

        pthread_mutex_destroy(&queue.lock);

        for(size_t j = 0;j < num_chunks;j++){//主线程按 j = 0,1,2,... 顺序输出,保持顺序
            output_result_with_merge(&tasks[j].result,&global_count,&global_ch);
            free_result(&tasks[j].result);
        }

        free(threads);
        free(tasks);

        munmap(data,st.st_size);//使用 munmap() 解除映射，释放内存资源。参数说明：
        // data: 指向要解除映射的内存区域的指针。
        // st.st_size: 要解除映射的长度，必须与 mmap() 时的长度一致。
        close(fd);//关闭文件描述符，释放系统资源。
    }

    if(global_count > 0){
        write_run(global_count,global_ch);
    }

    return 0;
}


