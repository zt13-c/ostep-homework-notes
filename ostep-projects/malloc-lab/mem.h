/*
#ifndef MEM_H
#define MEM_H
...
#endif
C 语言中标准的“防止重复包含”机制。如果这个头文件在同一个项目里被多次 #include，这三行代码能确保它内部的内容只被编译器处理一次，从而避免“变量/函数重复定义”的错误。
 */
#ifndef MEM_H
#define MEM_H

//内存分配策略宏
#define FIRSTFIT 0 //FIRSTFIT (首次适应)：从头开始找，遇到第一个足够大的空闲块就分配。速度快。
#define BESTFIT  1 //BESTFIT (最佳适应)：遍历所有空闲块，找出最小的足够大的块。节省内存，但速度慢。
#define WORSTFIT 2 //WORSTFIT (最差适应)：遍历所有空闲块，找出最大的空闲块。可能导致大量小碎片，但速度快。

//错误处理宏
#define E_BAD_ARGS 1 //E_BAD_ARGS：错误码 1，表示调用函数时传入了错误的参数（比如请求分配负数大小的内存，或者传入了未知的分配策略）。
#define E_NO_SPACE 2 //E_NO_SPACE：错误码 2，表示内存不够了，无法满足这次分配请求。

extern int m_error; 
/*
声明了一个全局变量 m_error。这个变量真正是在对应的 .c 文件（比如 mem.c）里定义的。当你的库函数（mem.c）发生错误时，你需要把相应的错误码（1 或 2）赋给这个变量，
这样调用者（main 函数等）就能检查 m_error 来知道究竟出了什么错
如果不用 extern 声明，编译器会认为 m_error 是一个局部变量，而不是全局变量，
现在，你有两个代码文件 mem.c（你的库）和 main.c（测试你的库的程序），它们都在开头写了 #include "mem.h"。当编译器处理时，它会把头文件的内容复制过去。
于是：mem.c 里出现了一个 int m_error;main.c 里也出现了一个 int m_error;这下麻烦了！
当你最后把这两个文件打包合并（链接，Linking）成一个可执行程序时，系统会发现有两个同名的 m_error 变量，它不知道该用哪一个，于是就会无情地报错：“multiple definition of m_error”（多重定义错误）。
*/

//核心功能接口
int Mem_Init(int sizeOfRegion);//初始化你的内存管理器。通常会向操作系统申请一大块内存（比如使用 mmap），大小为 sizeOfRegion。
void *Mem_Alloc(int size, int style);//相当于标准库里的 malloc。请求分配 size 字节的内存，并使用 style（即上面的 0, 1, 2）指定的策略来寻找空闲块。
int Mem_Free(void *ptr);//相当于标准库里的 free。释放之前通过 Mem_Alloc 分配的内存，让它变回空闲状态。
void Mem_Dump(void);//这是一个调试函数。通常用于打印当前内存池的状态，比如遍历你的空闲链表（Free List），打印出当前有哪些内存块是空闲的、哪些是已分配的、它们的大小和地址等，方便你排查 bug。

#endif