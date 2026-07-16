#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include "mem.h"

typedef struct block_header{
    int size; //表示这个块中 用户可用区域 的大小，不包括头部本身。
    int free; //是否空闲,1表示空闲，0表示已分配
    struct block_header *next; //指向下一个块的指针,用来形成链表
}block_header_t;

//全局变量，指向内存池的起始地址
int m_error = 0; //全局错误码变量，初始为0，表示没有错误

static block_header_t *head = NULL;//head 指向整个内存块链表的第一个块。之所以定义为 static，是为了限制它的作用域，只能在 mem.c 文件中访问，防止其他文件误用或修改这个指针。
static int initialized = 0; //标记内存池是否已经初始化过，0表示未初始化，1表示已初始化,用来防止 Mem_Init() 被调用多次。
static int region_size = 0; //记录总共向 OS 申请了多少字节。

//实现8字节对齐的辅助函数
static int align8(int size) {
    if(size % 8 == 0) return size;
    return ((size / 8) + 1) * 8;
}

//分裂空闲块的辅助函数
static void split_block(block_header_t *current, int aligned_size) {
    block_header_t *new_block = (block_header_t *)((char *)(current + 1) + aligned_size); //计算新块的地址,之所以要转化为 char * 是因为指针运算是基于它所指向的数据类型的大小进行的，而我们需要按字节计算地址偏移。
    new_block->size = current->size - aligned_size - sizeof(block_header_t); //计算新块的大小,新块的大小 = 当前块的大小 - 用户请求的大小 - 头部的大小
    new_block->free = 1; //新块是空闲的
    new_block->next = current->next; //新块的下一个块指针指向当前块的下一个块
    current->size = aligned_size; //更新当前块的大小为用户请求的大小
    current->next = new_block; //当前块的下一个块指针指向新块
}

//合并相邻空闲块的辅助函数
static void coalesce(void){
    block_header_t *current = head;
    while(current != NULL && current->next != NULL){
        if(current->free && current->next->free){ //如果当前块和下一个块都是空闲的，则合并它们
            current->size += sizeof(block_header_t) + current->next->size; //合并后的大小 = 当前块的大小 + 头部的大小 + 下一个块的大小
            current->next = current->next->next; //跳过下一个块，指向下下个块
            //注意：这里不需要移动 current 指针，因为我们可能还需要继续检查合并后的块和它的下一个块是否也可以合并
        } else {
            current = current->next; //移动到下一个块
        }
    }
}

//根据分配策略查找合适的空闲块的辅助函数
static block_header_t *find_block(int aligned_size, int style) {
    block_header_t *curr = head;
    block_header_t *chosen = NULL;

    while (curr != NULL) {
        if (curr->free && curr->size >= aligned_size) {
            if (style == FIRSTFIT) {
                return curr;
            }

            if (style == BESTFIT) {
                if (chosen == NULL || curr->size < chosen->size) {
                    chosen = curr;
                }
            }

            if (style == WORSTFIT) {
                if (chosen == NULL || curr->size > chosen->size) {
                    chosen = curr;
                }
            }
        }

        curr = curr->next;
    }

    return chosen;
}

//根据用户指针查找对应块头部是否合法
static block_header_t *find_block_by_user_ptr(void *ptr) {
    block_header_t *curr = head;

    while (curr != NULL) {
        if ((void *)(curr + 1) == ptr) {
            return curr;
        }

        curr = curr->next;
    }

    return NULL;
}

//实现Mem_Init函数
int Mem_Init(int sizeOfRegion) {
    if(initialized || sizeOfRegion <= 0) {//如果已经初始化过，或者 sizeOfRegion <= 0，则返回错误码 E_BAD_ARGS
        m_error = E_BAD_ARGS; 
        return -1;
    }
    //按页大小向上取整
    int pagesize = getpagesize();
    if(sizeOfRegion % pagesize != 0) {
        sizeOfRegion = ((sizeOfRegion / pagesize) + 1) * pagesize;
    }
    //使用 mmap 向操作系统申请一块内存区域
    void *ptr = mmap(NULL,//第一个参数为 NULL，表示让操作系统选择内存地址
                     sizeOfRegion,//第二个参数为 sizeOfRegion，表示申请的内存大小
                     PROT_READ | PROT_WRITE,//第三个参数为 PROT_READ | PROT_WRITE，表示申请的内存区域可读可写
                     MAP_PRIVATE | MAP_ANONYMOUS,//第四个参数为 MAP_PRIVATE | MAP_ANONYMOUS，表示申请的内存区域是私有的，并且不与任何文件关联
                     -1,//第五个参数为 -1，表示不与任何文件关联
                     0);//第六个参数为 0，表示偏移量为 0
    if(ptr == MAP_FAILED) {//如果 mmap 失败，则返回错误码 E_NO_SPACE,其中 MAP_FAILED 是 mmap 失败时返回的特殊指针值，通常定义为 (void *) -1。
        m_error = E_NO_SPACE;
        return -1;
    }

    //初始化内存池的头部信息
    head = (block_header_t *)ptr; //将 mmap 返回的指针强制转换为 block_header_t 类型，并赋值给 head，表示内存池的起始地址
    //初始化第一个大空闲块的头部信息
    head->size = sizeOfRegion - sizeof(block_header_t); //第一个大空闲块的大小为 sizeOfRegion 减去头部的大小
    head->free = 1; //第一个大空闲块是空闲的
    head->next = NULL; //第一个大空闲块没有下一个块，所以 next 指针为 NULL

    region_size = sizeOfRegion; //记录总共向 OS 申请了多少字节
    initialized = 1; //标记内存池已经初始化过
    return 0; //返回 0，表示初始化成功
}

void *Mem_Alloc(int size, int style) {
    if(!initialized || size <= 0) {//如果没有初始化过，或者 size <= 0，则返回错误码 E_BAD_ARGS
        m_error = E_BAD_ARGS;
        return NULL;
    }
    //先实现只有 FIRSTFIT 策略的分配
    if(style != FIRSTFIT && style != BESTFIT && style != WORSTFIT) {
        m_error = E_BAD_ARGS;
        return NULL;
    }
    int aligned_size = align8(size); //将 size 按 8 字节对齐

    block_header_t *block = find_block(aligned_size, style); //根据分配策略查找合适的空闲块

    if(block == NULL) { //如果没有找到合适的空闲块，则返回错误码 E_NO_SPACE
        m_error = E_NO_SPACE;
        return NULL;
    }

    if(block->size >= aligned_size + sizeof(block_header_t) + 8){ //如果当前块的大小大于等于用户请求的大小 + 头部的大小 + 8 字节（在 64 位系统中，内存通常要求至少 8 字节对齐，所以一个合法的空闲块，其可用数据区最少也得是 8 个字节），则分裂当前块
        split_block(block, aligned_size);
    }
    block->free = 0; //标记为已分配
    return (void *)(block + 1); //返回用户可用区域的指针，即头部之后的地址,block + 1 的意思是将 block 指针向后移动一个 block_header_t 的大小，这样就指向了用户可用区域的起始地址。
    //这是为什么？因为在 C 语言中，指针的算术运算是基于它所指向的数据类型的大小进行的。假设 block_header_t 的大小为 16 字节（这取决于具体的编译器和平台），那么 block + 1 实际上会将 block 指针向后移动 16 字节，从而指向紧接在头部之后的内存区域，也就是用户可用的内存块的起始地址。
}

int Mem_Free(void *ptr) {
    if(!initialized || ptr == NULL) {//如果没有初始化过，或者 ptr 为 NULL，则返回错误码 E_BAD_ARGS
        m_error = E_BAD_ARGS;
        return -1;
    }
    block_header_t *block = find_block_by_user_ptr(ptr); //根据用户指针查找对应块头部是否合法
    if (block == NULL) {
        m_error = E_BAD_ARGS;
        return -1;
    }
    if (block->free) { //如果当前块已经是空闲的，则返回错误码 E_BAD_ARGS
        m_error = E_BAD_ARGS;
        return -1;
    }
    block->free = 1; //标记为已释放
    coalesce(); //尝试合并相邻的空闲块
    return 0;
}

//实现Mem_Dump函数
void Mem_Dump(void) {
    block_header_t *current = head; //从链表的头部开始遍历
    int block_num = 0; //块编号，从 0 开始

    printf("Memory Dump:\n");
    printf("region_size: %d\n", region_size);

    while(current != NULL) { //遍历整个链表
        printf("Block %d: Address: %p, Size: %d, Free: %d, Next: %p\n", block_num, (void *)current, current->size, current->free, (void *)current->next);
        current = current->next; //移动到下一个块
        block_num++; //块编号加 1
    }
}




/*
这个代码中大量使用 static 是模块化设计和封装的体现，主要基于以下几个重要原因：

1. 限制作用域，实现信息隐藏
static 将变量和函数的作用域限制在 mem.c 文件内部，其他文件（如 main.c、test.c）无法直接访问：

    static block_header_t *head = NULL;      // 链表头，外部不可见
    static int initialized = 0;              // 初始化标志，外部不可见
    static int region_size = 0;              // 内存池大小，外部不可见

如果不用 static，这些全局变量会变成全局可见，任何文件都能修改它们，容易引发难以追踪的bug。

2. 防止命名冲突
在大型项目中，多个文件可能定义同名函数。static 函数只在当前文件可见，不会与其他文件的同名函数冲突：

    static int align8(int size) { ... }
    static void split_block(block_header_t *current, int aligned_size) { ... }
    static void coalesce(void) { ... }
    static block_header_t *find_block(...) { ... }

这些辅助函数都是内部实现细节，不需要对外暴露。

3. 只暴露必要的接口
用户只需要知道 mem.h 中声明的公开接口：

    Mem_Init()

    Mem_Alloc()

    Mem_Free()

    Mem_Dump()

而内部的实现细节（链表操作、分裂合并、查找算法等）用 static 隐藏，降低使用复杂度。

4. 增强代码安全性
外部文件无法直接修改 head 指针，必须通过公开的 Mem_Alloc()、Mem_Free() 等函数操作，保证了内存管理逻辑的正确性：

    // 外部无法这样做（编译会报错）：
    extern block_header_t *head;  // 即使声明也无法访问
    head->free = 0;               // 错误：未定义引用
    
5. 编译器优化
static 函数因为是内部调用，编译器可以进行更激进的优化（如内联展开），提升性能。
*/


    