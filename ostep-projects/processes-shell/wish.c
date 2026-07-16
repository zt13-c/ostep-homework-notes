#define _GNU_SOURCE
/*
 * _GNU_SOURCE:
 * 这个宏要放在所有 #include 前面。
 * 这里主要是为了使用 strsep()、getline() 等 GNU 扩展函数。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>

/*
 * 一些最大容量限制。
 * 这个项目不要求你写成完全动态扩容，所以用固定数组即可。
 */
#define MAX_PATHS 128       // path 命令最多保存多少个搜索路径
#define MAX_PATH_LEN 256    // 拼接出的完整路径最大长度，例如 /bin/ls
#define MAX_ARGS 128        // 一条命令最多多少个参数
#define MAX_PIDS 128        // 一行中用 & 并行启动的最大子进程数量

/*
 * shell 自己维护的搜索路径。
 *
 * 例如用户输入：
 * path /bin /usr/bin
 *
 * 那么：
 * paths[0] = "/bin"
 * paths[1] = "/usr/bin"
 * path_count = 2
 */
char *paths[MAX_PATHS];
int path_count = 0;


/*
 * 统一错误输出函数。
 *
 * OSTEP shell 项目要求所有错误都输出同一句：
 * "An error has occurred\n"
 *
 * 注意：错误要输出到 stderr，而不是 stdout。
 */
void print_error(void) {
    char error_message[30] = "An error has occurred\n";
    fprintf(stderr, "%s", error_message);
}


/*
 * 初始化 shell 的默认路径。
 *
 * 项目要求 wish 启动后，默认 path 中只有 /bin。
 * 所以刚启动时可以执行 ls、echo、cat 等 /bin 目录下的程序。
 */
void init_path(void) {
    paths[0] = strdup("/bin");   // 复制字符串 "/bin" 到堆区
    path_count = 1;
}


/*
 * 清空当前搜索路径。
 *
 * 因为 paths[i] 是 strdup() 申请出来的，所以清空前必须 free。
 *
 * 例如当前：
 * paths[0] = "/bin"
 * paths[1] = "/usr/bin"
 *
 * 执行 clear_path() 后：
 * path_count = 0
 */
void clear_path(void) {
    for (int i = 0; i < path_count; i++) {
        free(paths[i]);
        paths[i] = NULL;
    }

    path_count = 0;
}


/*
 * 处理内建命令 path。
 *
 * 用户输入：
 * path /bin /usr/bin
 *
 * parse_line 后：
 * args[0] = "path"
 * args[1] = "/bin"
 * args[2] = "/usr/bin"
 *
 * 所以真正的路径从 args[1] 开始。
 *
 * 注意：
 * path 命令不是追加路径，而是覆盖旧路径。
 * 所以第一步必须 clear_path()。
 */
void handle_path(char *args[], int cmd_argc) {
    clear_path();

    for (int i = 1; i < cmd_argc; i++) {
        paths[path_count] = strdup(args[i]);

        if (paths[path_count] == NULL) {
            print_error();
            exit(1);
        }

        path_count++;
    }
}


/*
 * 在当前 paths 数组中寻找某个外部命令。
 *
 * 例如用户输入：
 * ls -l
 *
 * cmd = "ls"
 *
 * 如果当前 paths 是：
 * paths[0] = "/bin"
 * paths[1] = "/usr/bin"
 *
 * 那么依次尝试：
 * /bin/ls
 * /usr/bin/ls
 *
 * access(full_path, X_OK) 用来检查该文件是否存在且可执行。
 *
 * 找到返回 1，找不到返回 0。
 */
int find_executable(char *cmd, char *full_path) {
    for (int i = 0; i < path_count; i++) {
        snprintf(full_path, MAX_PATH_LEN, "%s/%s", paths[i], cmd);

        if (access(full_path, X_OK) == 0) {
            return 1;
        }
    }

    return 0;
}


/*
 * 把一行命令按空格和 tab 拆成参数数组。
 *
 * 例如：
 * "echo hello world"
 *
 * 会被拆成：
 * args[0] = "echo"
 * args[1] = "hello"
 * args[2] = "world"
 * args[3] = NULL
 *
 * 返回值 argc = 3
 *
 * 注意：
 * strtok 会原地修改 line，把分隔符替换成 '\0'。
 */
int parse_line(char *line, char *args[]) {
    int argc = 0;

    char *token = strtok(line, " \t");

    while (token != NULL) {
        args[argc] = token;
        argc++;

        token = strtok(NULL, " \t");
    }

    args[argc] = NULL;
    return argc;
}


/*
 * 预处理重定向符号 >。
 *
 * 目的：
 * 让下面几种写法都能被统一处理：
 *
 * echo hello > out.txt
 * echo hello>out.txt
 * echo hello >out.txt
 * echo hello> out.txt
 *
 * 做法：
 * 遍历 cmd 字符串。
 * 每遇到一个 '>'，就替换成：
 * 空格 + > + 空格
 *
 * 例如：
 * "echo hello>out.txt"
 *
 * 变成：
 * "echo hello > out.txt"
 *
 * 这样后面的 parse_line() 仍然可以只按空格拆分。
 */
char *normalize_redirection(const char *cmd) {
    int extra = 0;

    /*
     * 每个 '>' 原本占 1 个字符。
     * 替换成 " > " 后占 3 个字符。
     * 所以每出现一个 '>'，需要额外多申请 2 个字符空间。
     */
    for (int i = 0; cmd[i] != '\0'; i++) {
        if (cmd[i] == '>') {
            extra += 2;
        }
    }

    /*
     * strlen(cmd)：原字符串长度
     * extra：因为给 > 两边补空格而增加的长度
     * +1：字符串结尾 '\0'
     */
    char *result = malloc(strlen(cmd) + extra + 1);

    if (result == NULL) {
        return NULL;
    }

    int j = 0;

    /*
     * 逐字符复制。
     * 普通字符原样复制。
     * 遇到 '>'，复制成 " > "。
     */
    for (int i = 0; cmd[i] != '\0'; i++) {
        if (cmd[i] == '>') {
            result[j] = ' ';
            j++;

            result[j] = '>';
            j++;

            result[j] = ' ';
            j++;
        } else {
            result[j] = cmd[i];
            j++;
        }
    }

    result[j] = '\0';
    return result;
}


/*
 * 解析重定向。
 *
 * 输入参数：
 * args      解析好的参数数组
 * cmd_argc 参数数量
 *
 * 输出参数：
 * output_file 如果存在重定向，则指向输出文件名；
 *             如果不存在重定向，则为 NULL。
 *
 * 例如：
 * echo hello > out.txt
 *
 * parse_line 后：
 * args[0] = "echo"
 * args[1] = "hello"
 * args[2] = ">"
 * args[3] = "out.txt"
 * cmd_argc = 4
 *
 * parse_redirection 后：
 * output_file = "out.txt"
 * args[0] = "echo"
 * args[1] = "hello"
 * args[2] = NULL
 * cmd_argc = 2
 *
 * 这样 execv 看到的参数就是：
 * echo hello
 *
 * 不会把 > 和 out.txt 传给 echo。
 */
int parse_redirection(char *args[], int *cmd_argc, char **output_file) {
    int redirect_index = -1;

    /*
     * 第一步：查找是否存在 >。
     *
     * 如果出现多个 >，例如：
     * echo hello > a.txt > b.txt
     *
     * 这是非法输入，返回 -1。
     */
    for (int i = 0; i < *cmd_argc; i++) {
        if (strcmp(args[i], ">") == 0) {
            if (redirect_index != -1) {
                return -1;
            }

            redirect_index = i;
        }
    }

    /*
     * 没有 >，说明没有重定向。
     */
    if (redirect_index == -1) {
        *output_file = NULL;
        return 0;
    }

    /*
     * > 不能出现在最前面。
     *
     * 例如：
     * > out.txt
     *
     * 这是非法的，因为没有命令。
     */
    if (redirect_index == 0) {
        return -1;
    }

    /*
     * > 后面必须刚好有一个文件名。
     *
     * 合法：
     * echo hello > out.txt
     *
     * 非法：
     * echo hello >
     * echo hello > a.txt b.txt
     */
    if (redirect_index + 2 != *cmd_argc) {
        return -1;
    }

    /*
     * 记录输出文件名。
     */
    *output_file = args[redirect_index + 1];

    /*
     * 截断 args。
     *
     * 原来：
     * args = ["echo", "hello", ">", "out.txt", NULL]
     *
     * 变成：
     * args = ["echo", "hello", NULL, "out.txt", NULL]
     *
     * execv 遇到 NULL 就停止读取参数。
     */
    args[redirect_index] = NULL;
    *cmd_argc = redirect_index;

    return 0;
}


/*
 * 执行外部命令。
 *
 * 外部命令包括：
 * ls
 * echo
 * cat
 * grep
 * sleep
 *
 * 不包括：
 * exit
 * cd
 * path
 *
 * 因为 exit、cd、path 会影响 shell 自己的状态，
 * 必须由 shell 父进程自己处理。
 *
 * 这个函数的核心流程：
 *
 * 1. 根据 path 查找可执行文件
 * 2. fork 创建子进程
 * 3. 子进程中处理重定向
 * 4. 子进程 execv 执行外部程序
 * 5. 父进程返回子进程 pid
 *
 * 注意：
 * 这个函数不 wait。
 * 因为为了支持 & 并行命令，要先启动多个子进程，
 * 再在 main 中统一 waitpid。
 */
pid_t execute_external(char *args[], char *output_file) {
    char full_path[MAX_PATH_LEN];

    /*
     * 先在 paths 中找命令。
     *
     * 例如 args[0] = "ls"
     * 可能找到 full_path = "/bin/ls"
     */
    if (find_executable(args[0], full_path) == 0) {
        print_error();
        return -1;
    }

    /*
     * fork 后会产生两个进程：
     *
     * rc < 0：fork 失败
     * rc == 0：当前在子进程中
     * rc > 0：当前在父进程中，rc 是子进程 pid
     */
    pid_t rc = fork();

    if (rc < 0) {
        print_error();
        return -1;
    } else if (rc == 0) {
        /*
         * 子进程部分。
         *
         * 如果有输出重定向，就在 execv 前改文件描述符。
         *
         * 为什么必须在子进程里做？
         * 因为只想让这个外部命令的输出进入文件，
         * 不想让 wish 自己的提示符也进入文件。
         */
        if (output_file != NULL) {
            /*
             * 打开输出文件。
             *
             * O_WRONLY：只写
             * O_CREAT：文件不存在就创建
             * O_TRUNC：文件存在就清空
             *
             * S_IRUSR | S_IWUSR：
             * 创建出来的文件，用户可读可写。
             */
            int fd = open(output_file,
                          O_WRONLY | O_CREAT | O_TRUNC,
                          S_IRUSR | S_IWUSR);

            if (fd < 0) {
                print_error();
                exit(1);
            }

            /*
             * dup2(fd, STDOUT_FILENO):
             * 把标准输出 fd 1 改到 output_file。
             *
             * dup2(fd, STDERR_FILENO):
             * 把标准错误 fd 2 也改到 output_file。
             *
             * 这样外部程序的普通输出和错误输出都会进入文件。
             */
            if (dup2(fd, STDOUT_FILENO) < 0 ||
                dup2(fd, STDERR_FILENO) < 0) {
                print_error();
                close(fd);
                exit(1);
            }

            /*
             * fd 已经复制到 1 和 2 上了。
             * 原始 fd 可以关闭，避免文件描述符泄漏。
             */
            close(fd);
        }

        /*
         * 用外部程序替换当前子进程。
         *
         * 例如：
         * execv("/bin/ls", args)
         *
         * 成功后，子进程就不再运行 wish 的代码，
         * 而是变成 /bin/ls。
         *
         * execv 成功不会返回。
         * 如果返回了，说明失败。
         */
        execv(full_path, args);

        /*
         * 只有 execv 失败时才会执行到这里。
         */
        print_error();
        exit(1);
    }

    /*
     * 父进程返回子进程 pid。
     *
     * main 函数会把这个 pid 保存起来，
     * 等一整行中的所有并行命令都启动后，再统一 waitpid。
     */
    return rc;
}


/*
 * 处理一条子命令。
 *
 * 为什么叫 single command？
 *
 * 因为一整行可能有多个命令，用 & 分隔：
 *
 * sleep 2 & echo hello & ls
 *
 * main 会先按 & 拆开：
 *
 * 子命令 1：sleep 2
 * 子命令 2：echo hello
 * 子命令 3：ls
 *
 * 每个子命令交给 handle_single_command() 处理。
 *
 * 返回值：
 *  0：正常处理完成
 * -1：遇到 exit，需要退出整个 shell
 */
int handle_single_command(char *cmd, pid_t pids[], int *pid_count) {
    /*
     * 第一步：预处理 >。
     *
     * 把：
     * echo hello>out.txt
     *
     * 转成：
     * echo hello > out.txt
     */
    char *normallized_cmd = normalize_redirection(cmd);

    if (normallized_cmd == NULL) {
        print_error();
        return 0;
    }

    /*
     * 第二步：把命令字符串拆成参数数组。
     */
    char *args[MAX_ARGS];
    int cmd_argc = parse_line(normallized_cmd, args);

    /*
     * 如果是空命令，直接忽略。
     *
     * 例如用户输入：
     * &
     *
     * 按 & 分割后可能出现空命令。
     */
    if (cmd_argc == 0) {
        free(normallized_cmd);
        return 0;
    }

    /*
     * 第三步：处理重定向。
     *
     * 如果命令里有 >，会把 output_file 设置成输出文件名，
     * 并从 args 中删掉 > 和文件名。
     */
    char *output_file = NULL;

    if (parse_redirection(args, &cmd_argc, &output_file) != 0) {
        print_error();
        free(normallized_cmd);
        return 0;
    }

    /*
     * 第四步：判断是否是内建命令 exit。
     *
     * exit 只能单独使用：
     * exit
     *
     * 不能：
     * exit 1
     * exit > out.txt
     */
    if (strcmp(args[0], "exit") == 0) {
        if (output_file != NULL || cmd_argc != 1) {
            print_error();
            free(normallized_cmd);
            return 0;
        } else {
            /*
             * 不直接 exit(0)，而是 return -1。
             *
             * 这样 main 可以知道：
             * 当前行中遇到了 exit，需要跳出主循环。
             */
            free(normallized_cmd);
            return -1;
        }
    }

    /*
     * 第五步：判断是否是内建命令 cd。
     *
     * cd 必须由 shell 自己执行，不能 fork 到子进程执行。
     *
     * 因为 cd 改变的是当前进程的工作目录。
     * 如果子进程 chdir，父进程 wish 的目录不会变。
     */
    else if (strcmp(args[0], "cd") == 0) {
        /*
         * cd 后面必须刚好一个目录参数。
         *
         * 合法：
         * cd /tmp
         *
         * 非法：
         * cd
         * cd /tmp /home
         * cd /tmp > out.txt
         */
        if (output_file != NULL || cmd_argc != 2) {
            print_error();
            free(normallized_cmd);
            return 0;
        } else {
            if (chdir(args[1]) != 0) {
                print_error();
            }

            free(normallized_cmd);
            return 0;
        }
    }

    /*
     * 第六步：判断是否是内建命令 path。
     *
     * path 修改的是 shell 自己维护的 paths 数组。
     * 所以它也必须由父进程自己执行。
     */
    else if (strcmp(args[0], "path") == 0) {
        /*
         * path 不允许重定向。
         */
        if (output_file != NULL) {
            print_error();
            free(normallized_cmd);
            return 0;
        } else {
            handle_path(args, cmd_argc);
            free(normallized_cmd);
            return 0;
        }
    }

    /*
     * 第七步：普通外部命令。
     *
     * 例如：
     * ls
     * echo hello
     * cat file.txt
     * sleep 2
     *
     * 这些命令通过 fork + execv 执行。
     */
    else {
        pid_t pid = execute_external(args, output_file);

        /*
         * 如果成功 fork 出子进程，就保存它的 pid。
         *
         * main 函数之后会统一 waitpid。
         */
        if (pid > 0) {
            pids[*pid_count] = pid;
            (*pid_count)++;
        }
    }

    /*
     * normallized_cmd 是 malloc 出来的。
     * parse_line 得到的 args 都指向它内部的片段。
     *
     * 到这里命令已经处理完，可以释放。
     */
    free(normallized_cmd);
    return 0;
}


/*
 * main 是整个 shell 的主循环。
 *
 * 它负责：
 *
 * 1. 判断交互模式还是批处理模式
 * 2. 初始化默认 path
 * 3. 循环读取命令行
 * 4. 按 & 拆成多个子命令
 * 5. 逐个处理子命令
 * 6. 等待所有外部子进程结束
 * 7. 释放资源并退出
 */
int main(int argc, char *argv[]) {
    /*
     * flag 用来记录是否遇到 exit。
     *
     * handle_single_command() 遇到合法 exit 时返回 -1。
     * main 收到 -1 后把 flag 置为 1，然后退出主循环。
     */
    int flag = 0;

    /*
     * input 表示 shell 的输入来源。
     *
     * 交互模式：
     * input = stdin
     *
     * 批处理模式：
     * input = fopen(batch_file, "r")
     */
    FILE *input = stdin;

    /*
     * interactive = 1：交互模式，需要打印 wish> 提示符
     * interactive = 0：批处理模式，不打印提示符
     */
    int interactive = 1;

    /*
     * 参数检查。
     *
     * 合法：
     * ./wish
     * ./wish batch.txt
     *
     * 非法：
     * ./wish a.txt b.txt
     */
    if (argc > 2) {
        print_error();
        exit(1);
    }

    /*
     * 批处理模式。
     *
     * 如果用户传入一个文件名，就从这个文件中读取命令。
     */
    if (argc == 2) {
        input = fopen(argv[1], "r");

        if (input == NULL) {
            print_error();
            exit(1);
        }

        interactive = 0;
    }

    /*
     * getline 使用的缓冲区。
     *
     * line = NULL, len = 0 时，
     * getline 会自动 malloc / realloc。
     */
    char *line = NULL;
    size_t len = 0;

    /*
     * 初始化默认搜索路径。
     */
    init_path();

    /*
     * shell 主循环。
     *
     * 每次循环读取一整行命令。
     */
    while (1) {
        /*
         * 交互模式下打印提示符。
         *
         * 批处理模式不打印。
         */
        if (interactive) {
            printf("wish> ");
            fflush(stdout);
        }

        /*
         * 从 input 读取一行。
         *
         * 交互模式 input 是 stdin。
         * 批处理模式 input 是 batch 文件。
         *
         * getline 返回 -1 表示读到 EOF 或出错。
         */
        if (getline(&line, &len, input) == -1) {
            break;
        }

        /*

        第一次调用：如果 line == NULL，getline 自动 malloc
        后续调用：如果 line 不是 NULL，getline 复用内存
        内存不足时：getline 用 realloc 扩大
        返回 -1 时：内存不会被释放
        第二次调用时，line 指向的地址不变，但内容会被 getline 覆盖！

        第一次调用: getline(&line, &len, stdin)
        → 输入 "hello"
        → line → [h][e][l][l][o][\n][\0]...

        第二次调用: getline(&line, &len, stdin)
        → 输入 "world"
        → line → [w][o][r][l][d][\n][\0]...  ← 旧内容被覆盖！
                ↑
            同一个地址，但内容变了

        fgets 方式
        char line[256];  // 固定大小，在栈上
        while (fgets(line, sizeof(line), stdin)) {
            // 处理 line...
        }
        不需要 free，因为 line 在栈上自动释放

        特性                 fgets() (C 标准库)                            getline() (POSIX 标准)
        缓冲区管理        程序员手动分配固定大小的数组                  函数自动动态分配和扩容（需手动释放）
        安全性        安全（防溢出），但超长行会被截断分次读取         极度安全，保证读取完整的一行，不会截断
        跨平台性           极佳（所有 C 编译器均支持）           一般（主要在 Linux/Unix 环境受支持，Windows/MSVC 默认没有）
        适用场景       内存受限或已知最大行长度的简单读取                 读取行长度未知、可能极长的数据文件

        */

        /*
         * 去掉行尾换行符。
         *
         * 例如：
         * "ls -l\n"
         *
         * 变成：
         * "ls -l"
         */
        line[strcspn(line, "\n")] = '\0';

        /*
         * 保存本行中所有外部命令 fork 出来的子进程 pid。
         *
         * 这样可以支持：
         * cmd1 & cmd2 & cmd3
         *
         * 先把它们都启动，再统一 waitpid。
         */
        pid_t pids[MAX_PIDS];
        int pid_count = 0;

        /*
         * 使用 strsep 按 & 拆分一行。
         *
         * 例如：
         * sleep 2 & echo hello & ls
         *
         * 会被拆成：
         * sleep 2
         * echo hello
         * ls
         */
        char *rest = line;
        char *cmd;

        while ((cmd = strsep(&rest, "&")) != NULL) {
            int result = handle_single_command(cmd, pids, &pid_count);

            /*
             * result == -1 表示遇到合法 exit。
             * 退出当前行处理，并准备退出整个 shell。
             */
            if (result == -1) {
                flag = 1;
                break;
            }
        }

        /*
         * 如果本行中遇到 exit，就退出主循环。
         */
        if (flag) {
            break;
        }

        /*
         * 等待这一行中启动的所有外部命令结束。
         *
         * 注意：
         * execute_external() 只 fork，不 wait。
         * waitpid 放在这里，才能实现 & 并行。
         */
        for (int i = 0; i < pid_count; i++) {
            waitpid(pids[i], NULL, 0);
        }
    }

    /*
     * 释放 getline 自动申请的缓冲区。
     */
    free(line);

    /*
     * 释放 path 命令中 strdup 出来的路径字符串。
     */
    clear_path();

    /*
     * 如果是批处理模式，关闭打开的 batch 文件。
     *
     * stdin 不需要手动关闭。
     */
    if (input != stdin) {
        fclose(input);
    }

    return 0;
}