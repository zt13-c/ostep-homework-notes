# OSTEP 第 39 章作业：文件系统 API 编程题笔记

## 0. 本组作业主题

这 4 道题主要围绕 UNIX/Linux 文件系统 API：

1. 自己实现一个简化版 `stat`
2. 自己实现一个简化版 `ls`
3. 自己实现一个简化版 `tail`
4. 自己实现一个简化版 `find`

涉及的核心接口包括：

~~~c
stat()
open()
read()
write()
close()
lseek()
opendir()
readdir()
closedir()
getcwd()
~~~

---

# 1. 作业 1：实现自己的 stat 工具

## 1.1 题目要求

实现一个自己的命令行工具 `stat`，对一个文件或目录调用 `stat()` 函数，将文件大小、分配的磁盘块数、引用数等信息打印出来。

运行方式类似：

~~~bash
./mystat file
./mystat dir
~~~

需要打印：

- inode 号
- 文件大小
- 分配的磁盘块数
- IO 块大小
- 硬链接数 / 引用计数
- 权限模式
- 用户 ID
- 用户组 ID
- 文件类型

---

## 1.2 参考代码

文件名：`mystat.c`

~~~c
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    struct stat st;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file-or-directory>\n", argv[0]);
        exit(1);
    }

    if (stat(argv[1], &st) == -1) {
        perror("stat");
        exit(1);
    }

    printf("Path: %s\n", argv[1]);
    printf("Inode: %lu\n", (unsigned long)st.st_ino);
    printf("Size: %lld bytes\n", (long long)st.st_size);
    printf("Blocks: %lld\n", (long long)st.st_blocks);
    printf("IO Block Size: %ld\n", (long)st.st_blksize);
    printf("Links: %lu\n", (unsigned long)st.st_nlink);
    printf("Mode: %o\n", st.st_mode);
    printf("UID: %u\n", st.st_uid);
    printf("GID: %u\n", st.st_gid);

    if (S_ISREG(st.st_mode)) {
        printf("Type: regular file\n");
    } else if (S_ISDIR(st.st_mode)) {
        printf("Type: directory\n");
    } else if (S_ISLNK(st.st_mode)) {
        printf("Type: symbolic link\n");
    } else {
        printf("Type: other\n");
    }

    return 0;
}
~~~

---

## 1.3 编译运行

~~~bash
gcc mystat.c -o mystat
./mystat file
./mystat .
./mystat dir
~~~

---

## 1.4 核心代码解释

### `struct stat st`

~~~c
struct stat st;
~~~

`struct stat` 用来保存文件元数据。

常用字段：

~~~c
st.st_ino       // inode 号
st.st_size      // 文件大小，单位字节
st.st_blocks    // 分配的磁盘块数
st.st_blksize   // 文件系统推荐 IO 块大小
st.st_nlink     // 硬链接数 / 引用计数
st.st_mode      // 文件类型和权限
st.st_uid       // 所有者用户 ID
st.st_gid       // 所属组 ID
~~~

### `stat(argv[1], &st)`

~~~c
if (stat(argv[1], &st) == -1) {
    perror("stat");
    exit(1);
}
~~~

含义：

1. 根据路径 `argv[1]` 找到文件或目录；
2. 读取它的元数据；
3. 把结果填入 `st` 结构体。

如果失败，`stat()` 返回 `-1`。

### 判断文件类型

~~~c
if (S_ISREG(st.st_mode)) {
    printf("Type: regular file\n");
} else if (S_ISDIR(st.st_mode)) {
    printf("Type: directory\n");
} else if (S_ISLNK(st.st_mode)) {
    printf("Type: symbolic link\n");
} else {
    printf("Type: other\n");
}
~~~

`st.st_mode` 同时包含：

- 文件类型；
- 文件权限。

常用判断宏：

| 宏 | 含义 |
|---|---|
| `S_ISREG()` | 是否为普通文件 |
| `S_ISDIR()` | 是否为目录 |
| `S_ISLNK()` | 是否为符号链接 |

---

## 1.5 目录引用计数如何变化

目录的引用计数字段是：

~~~c
st.st_nlink
~~~

对于普通文件，`st_nlink` 表示有多少个硬链接指向这个 inode。

对于目录，一个新建目录通常至少有两个引用：

~~~text
.   指向自己
父目录中的目录项也指向自己
~~~

所以新建空目录的 link count 通常是：

~~~text
2
~~~

例如：

~~~bash
mkdir dir
./mystat dir
~~~

可能输出：

~~~text
Links: 2
~~~

---

## 1.6 创建子目录时 link count 变化

假设：

~~~bash
mkdir dir
./mystat dir
~~~

此时：

~~~text
Links: 2
~~~

然后：

~~~bash
mkdir dir/sub
./mystat dir
~~~

此时 `dir` 的 link count 通常变成：

~~~text
Links: 3
~~~

原因是：

~~~text
dir/sub/.. 指向父目录 dir
~~~

所以父目录 `dir` 多了一个来自子目录 `sub` 的 `..` 引用。

因此：

~~~text
目录 link count = 2 + 直接子目录数量
~~~

例如：

~~~bash
mkdir dir/a
mkdir dir/b
mkdir dir/c
./mystat dir
~~~

此时：

~~~text
Links: 5
~~~

因为：

~~~text
2 + 3 个直接子目录 = 5
~~~

---

## 1.7 创建普通文件不会改变目录 link count

例如：

~~~bash
touch dir/file1
touch dir/file2
./mystat dir
~~~

`dir` 的 link count 通常不会增加。

原因是：

~~~text
普通文件没有 .. 指向父目录
~~~

总结：

~~~text
新增普通文件：父目录 link count 不变
新增子目录：父目录 link count 加 1
删除子目录：父目录 link count 减 1
~~~

---

# 2. 作业 2：实现自己的 ls 工具

## 2.1 题目要求

编写一个程序，列出指定目录内容。

需要支持：

~~~bash
./myls
./myls directory
./myls -l
./myls -l directory
~~~

要求：

- 不带参数时，列出当前目录；
- 带目录参数时，列出指定目录；
- 不带 `-l` 时，只打印文件名；
- 带 `-l` 时，打印权限、所有者、所属组、大小、修改时间等详细信息。

---

## 2.2 参考代码

文件名：`myls.c`

~~~c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <limits.h>

void print_permissions(mode_t mode) {
    if (S_ISDIR(mode)) {
        printf("d");
    } else if (S_ISLNK(mode)) {
        printf("l");
    } else {
        printf("-");
    }

    printf("%c", (mode & S_IRUSR) ? 'r' : '-');
    printf("%c", (mode & S_IWUSR) ? 'w' : '-');
    printf("%c", (mode & S_IXUSR) ? 'x' : '-');

    printf("%c", (mode & S_IRGRP) ? 'r' : '-');
    printf("%c", (mode & S_IWGRP) ? 'w' : '-');
    printf("%c", (mode & S_IXGRP) ? 'x' : '-');

    printf("%c", (mode & S_IROTH) ? 'r' : '-');
    printf("%c", (mode & S_IWOTH) ? 'w' : '-');
    printf("%c", (mode & S_IXOTH) ? 'x' : '-');
}

void print_long_info(const char *dirpath, const char *filename) {
    struct stat st;
    char fullpath[PATH_MAX];

    snprintf(fullpath, sizeof(fullpath), "%s/%s", dirpath, filename);

    if (stat(fullpath, &st) == -1) {
        perror("stat");
        return;
    }

    print_permissions(st.st_mode);

    printf(" %lu", (unsigned long)st.st_nlink);

    struct passwd *pw = getpwuid(st.st_uid);
    struct group *gr = getgrgid(st.st_gid);

    printf(" %s", pw ? pw->pw_name : "unknown");
    printf(" %s", gr ? gr->gr_name : "unknown");

    printf(" %lld", (long long)st.st_size);

    char timebuf[64];
    struct tm *tm_info = localtime(&st.st_mtime);
    strftime(timebuf, sizeof(timebuf), "%b %d %H:%M", tm_info);

    printf(" %s", timebuf);

    printf(" %s\n", filename);
}

void list_directory(const char *dirpath, int long_format) {
    DIR *dir;
    struct dirent *entry;

    dir = opendir(dirpath);
    if (dir == NULL) {
        perror("opendir");
        exit(1);
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;
        }

        if (long_format) {
            print_long_info(dirpath, entry->d_name);
        } else {
            printf("%s\n", entry->d_name);
        }
    }

    closedir(dir);
}

int main(int argc, char *argv[]) {
    int long_format = 0;
    char dirpath[PATH_MAX];

    if (argc == 1) {
        if (getcwd(dirpath, sizeof(dirpath)) == NULL) {
            perror("getcwd");
            exit(1);
        }
    } else if (argc == 2) {
        if (strcmp(argv[1], "-l") == 0) {
            long_format = 1;

            if (getcwd(dirpath, sizeof(dirpath)) == NULL) {
                perror("getcwd");
                exit(1);
            }
        } else {
            strncpy(dirpath, argv[1], sizeof(dirpath));
            dirpath[sizeof(dirpath) - 1] = '\0';
        }
    } else if (argc == 3) {
        if (strcmp(argv[1], "-l") == 0) {
            long_format = 1;
            strncpy(dirpath, argv[2], sizeof(dirpath));
            dirpath[sizeof(dirpath) - 1] = '\0';
        } else {
            fprintf(stderr, "Usage: %s [-l] [directory]\n", argv[0]);
            exit(1);
        }
    } else {
        fprintf(stderr, "Usage: %s [-l] [directory]\n", argv[0]);
        exit(1);
    }

    list_directory(dirpath, long_format);

    return 0;
}
~~~

---

## 2.3 编译运行

~~~bash
gcc myls.c -o myls

./myls
./myls /home
./myls -l
./myls -l /home
~~~

---

## 2.4 程序整体思路

~~~text
myls = opendir() + readdir() + stat()
~~~

流程：

1. 判断是否带 `-l` 参数；
2. 判断是否指定目录；
3. 如果没指定目录，用 `getcwd()` 获取当前目录；
4. 用 `opendir()` 打开目录；
5. 用 `readdir()` 逐项读取目录项；
6. 普通模式只打印文件名；
7. `-l` 模式调用 `stat()` 打印详细信息；
8. 用 `closedir()` 关闭目录。

---

## 2.5 `getcwd()` 解释

代码：

~~~c
if (getcwd(dirpath, sizeof(dirpath)) == NULL) {
    perror("getcwd");
    exit(1);
}
~~~

`getcwd()` 的意思是：

~~~text
get current working directory
获取当前工作目录
~~~

例如当前终端路径是：

~~~bash
/home/czt/ostep-homework-notes/ch39
~~~

执行：

~~~c
getcwd(dirpath, sizeof(dirpath));
~~~

之后：

~~~text
dirpath = "/home/czt/ostep-homework-notes/ch39"
~~~

`sizeof(dirpath)` 表示 `dirpath` 数组的大小，防止写入越界。

---

## 2.6 `strncpy()` 解释

代码：

~~~c
strncpy(dirpath, argv[1], sizeof(dirpath));
dirpath[sizeof(dirpath) - 1] = '\0';
~~~

含义：

1. 把用户输入的目录路径 `argv[1]` 复制到 `dirpath`；
2. 最多复制 `sizeof(dirpath)` 个字符；
3. 最后强制加 `'\0'`，保证 C 字符串正常结束。

不用 `strcpy()` 的原因是：

~~~text
strcpy() 不检查目标数组大小，用户输入太长可能导致数组越界。
~~~

---

## 2.7 `print_permissions()` 函数解释

这个函数打印类似 `ls -l` 最前面的权限字符串：

~~~text
-rw-r--r--
drwxr-xr-x
lrwxrwxrwx
~~~

第一位表示文件类型：

| 字符 | 含义 |
|---|---|
| `-` | 普通文件 |
| `d` | 目录 |
| `l` | 符号链接 |

后面 9 位分别表示：

~~~text
用户权限  组权限  其他人权限
rwx       rwx     rwx
~~~

---

## 2.8 `print_long_info()` 函数解释

函数：

~~~c
void print_long_info(const char *dirpath, const char *filename)
~~~

作用：

~~~text
在 -l 模式下，打印某个文件的一行详细信息。
~~~

类似：

~~~bash
-rw-r--r-- 1 czt czt 1234 Jun 22 10:12 myls.c
~~~

### 拼接完整路径

~~~c
snprintf(fullpath, sizeof(fullpath), "%s/%s", dirpath, filename);
~~~

`readdir()` 只返回文件名，例如：

~~~text
a.txt
~~~

但 `stat()` 需要能找到文件的路径。

如果目录是：

~~~text
testdir
~~~

那么应该对：

~~~text
testdir/a.txt
~~~

调用 `stat()`。

所以要拼接：

~~~text
dirpath + "/" + filename
~~~

### 调用 stat

~~~c
if (stat(fullpath, &st) == -1) {
    perror("stat");
    return;
}
~~~

获取文件元数据，填入 `st`。

### 打印硬链接数

~~~c
printf(" %lu", (unsigned long)st.st_nlink);
~~~

打印 link count。

### 打印用户名和组名

~~~c
struct passwd *pw = getpwuid(st.st_uid);
struct group *gr = getgrgid(st.st_gid);
~~~

`stat()` 得到的是数字 ID：

~~~c
st.st_uid
st.st_gid
~~~

`getpwuid()` 把 UID 转成用户名。

`getgrgid()` 把 GID 转成组名。

### 打印文件大小

~~~c
printf(" %lld", (long long)st.st_size);
~~~

`st.st_size` 是文件大小，单位字节。

### 打印修改时间

~~~c
struct tm *tm_info = localtime(&st.st_mtime);
strftime(timebuf, sizeof(timebuf), "%b %d %H:%M", tm_info);
~~~

`st.st_mtime` 是文件最后修改时间。

`localtime()` 把时间戳转换成本地时间。

`strftime()` 格式化成类似：

~~~text
Jun 22 10:12
~~~

---

## 2.9 `list_directory()` 函数解释

函数：

~~~c
void list_directory(const char *dirpath, int long_format)
~~~

作用：

1. 打开目录；
2. 读取每个目录项；
3. 根据是否 `-l` 决定打印文件名还是详细信息；
4. 关闭目录。

核心代码：

~~~c
while ((entry = readdir(dir)) != NULL) {
    if (entry->d_name[0] == '.') {
        continue;
    }

    if (long_format) {
        print_long_info(dirpath, entry->d_name);
    } else {
        printf("%s\n", entry->d_name);
    }
}
~~~

这里跳过了以 `.` 开头的隐藏文件，例如：

~~~text
.
..
.git
.hidden
~~~

这和普通 `ls` 默认行为类似。

---

# 3. 作业 3：实现自己的 tail 工具

## 3.1 题目要求

编写一个程序，输出一个文件的最后几行。

运行方式：

~~~bash
./mytail -5 test.txt
~~~

表示输出 `test.txt` 最后 5 行。

要求使用：

~~~c
stat()
lseek()
open()
read()
close()
~~~

---

## 3.2 核心思路

不要从头读完整个文件。

更好的方法是：

1. 用 `open()` 打开文件；
2. 用 `stat()` 获取文件大小；
3. 从文件末尾开始向前扫描；
4. 统计换行符 `\n`；
5. 找到最后 `n` 行的起始位置；
6. 用 `lseek()` 跳到该位置；
7. 用 `read()` 顺序读到文件末尾；
8. 用 `write()` 输出到屏幕；
9. 用 `close()` 关闭文件。

---

## 3.3 参考代码

文件名：`mytail.c`

~~~c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#define BUF_SIZE 4096

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s -n file\n", argv[0]);
        exit(1);
    }

    int n = atoi(argv[1] + 1);
    if (argv[1][0] != '-' || n <= 0) {
        fprintf(stderr, "Usage: %s -n file\n", argv[0]);
        exit(1);
    }

    char *filename = argv[2];

    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("open");
        exit(1);
    }

    struct stat st;
    if (stat(filename, &st) < 0) {
        perror("stat");
        close(fd);
        exit(1);
    }

    off_t file_size = st.st_size;
    off_t pos = file_size;
    int lines = 0;
    char c;

    while (pos > 0) {
        pos--;

        if (lseek(fd, pos, SEEK_SET) < 0) {
            perror("lseek");
            close(fd);
            exit(1);
        }

        if (read(fd, &c, 1) != 1) {
            perror("read");
            close(fd);
            exit(1);
        }

        if (c == '\n') {
            if (pos != file_size - 1) {
                lines++;
            }

            if (lines == n) {
                pos++;
                break;
            }
        }
    }

    if (pos == 0) {
        lseek(fd, 0, SEEK_SET);
    } else {
        lseek(fd, pos, SEEK_SET);
    }

    char buf[BUF_SIZE];
    ssize_t bytes_read;

    while ((bytes_read = read(fd, buf, BUF_SIZE)) > 0) {
        if (write(STDOUT_FILENO, buf, bytes_read) != bytes_read) {
            perror("write");
            close(fd);
            exit(1);
        }
    }

    if (bytes_read < 0) {
        perror("read");
        close(fd);
        exit(1);
    }

    close(fd);
    return 0;
}
~~~

---

## 3.4 编译运行

~~~bash
gcc mytail.c -o mytail
./mytail -5 test.txt
~~~

---

## 3.5 参数解析

命令：

~~~bash
./mytail -5 test.txt
~~~

对应：

~~~c
argv[0] = "./mytail"
argv[1] = "-5"
argv[2] = "test.txt"
~~~

代码：

~~~c
int n = atoi(argv[1] + 1);
~~~

解释：

- `argv[1]` 指向字符串 `"-5"`；
- `argv[1] + 1` 跳过 `'-'`，指向 `"5"`；
- `atoi("5")` 转成整数 `5`。

所以：

~~~c
int n = atoi(argv[1] + 1);
~~~

意思是：

~~~text
把命令行参数 -5 中的数字部分取出来，转成整数 5。
~~~

---

## 3.6 获取文件大小

~~~c
struct stat st;
stat(filename, &st);
off_t file_size = st.st_size;
~~~

`stat()` 获取文件元数据。

`st.st_size` 是文件大小，单位字节。

因为要从文件末尾往前扫描，所以需要知道文件大小。

---

## 3.7 从文件末尾向前扫描

~~~c
off_t pos = file_size;
int lines = 0;
char c;
~~~

- `pos` 表示当前扫描位置；
- 初始值是文件末尾；
- `lines` 用来统计已经遇到多少个换行边界；
- `c` 用来保存每次读到的字符。

核心循环：

~~~c
while (pos > 0) {
    pos--;

    lseek(fd, pos, SEEK_SET);
    read(fd, &c, 1);

    if (c == '\n') {
        ...
    }
}
~~~

含义：

1. 每次向前移动 1 字节；
2. 用 `lseek()` 跳到该位置；
3. 用 `read()` 读 1 个字符；
4. 如果是换行符 `\n`，说明可能到达一行的分界。

---

## 3.8 为什么跳过文件末尾的换行符

代码：

~~~c
if (c == '\n') {
    if (pos != file_size - 1) {
        lines++;
    }
}
~~~

很多文本文件最后一个字符就是换行符 `\n`。

例如：

~~~text
line1\n
line2\n
line3\n
~~~

最后一个字符是 `\n`，它只是最后一行的结束符，不应该算作“已经越过了一行”。

所以：

~~~c
if (pos != file_size - 1)
~~~

表示如果这个换行符不是文件最后一个字符，才计数。

---

## 3.9 为什么找到后要 `pos++`

代码：

~~~c
if (lines == n) {
    pos++;
    break;
}
~~~

从后往前找到第 `n` 个换行符后，这个换行符本身属于上一行的末尾。

真正需要打印的位置应该从它后面一个字符开始。

所以要：

~~~c
pos++;
~~~

---

## 3.10 为什么后面用 while 继续 read

代码：

~~~c
while ((bytes_read = read(fd, buf, BUF_SIZE)) > 0) {
    write(STDOUT_FILENO, buf, bytes_read);
}
~~~

这里用 `while` 是因为文件剩余内容可能超过一个缓冲区大小。

如果：

~~~c
#define BUF_SIZE 4096
~~~

而剩余内容是 10000 字节，则可能需要多次读取：

~~~text
第 1 次 read：4096 字节
第 2 次 read：4096 字节
第 3 次 read：1808 字节
第 4 次 read：0 字节，EOF
~~~

`read()` 返回值含义：

| 返回值 | 含义 |
|---|---|
| `> 0` | 实际读到的字节数 |
| `= 0` | 读到文件末尾 EOF |
| `< 0` | 读取错误 |

所以：

~~~c
while ((bytes_read = read(fd, buf, BUF_SIZE)) > 0)
~~~

表示：

~~~text
只要还能读到数据，就继续读并输出。
~~~

---

## 3.11 程序整体流程

以：

~~~bash
./mytail -2 file
~~~

为例。

如果文件内容是：

~~~text
aaa
bbb
ccc
ddd
eee
~~~

程序做：

~~~text
1. open 打开 file
2. stat 获取文件大小
3. 从文件末尾开始往前找换行符
4. 找到最后 2 行的起始位置
5. lseek 到该位置
6. 从该位置 read 到文件末尾
7. write 到标准输出
8. close 文件
~~~

输出：

~~~text
ddd
eee
~~~

---

# 4. 作业 4：实现递归查找 myfind

## 4.1 题目要求

编写一个程序，打印指定目录树下所有的文件和目录名。

运行方式：

~~~bash
./myfind
~~~

从当前目录 `.` 开始。

~~~bash
./myfind directory
~~~

从指定目录开始。

---

## 4.2 参考代码

文件名：`myfind.c`

~~~c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

void recursive_find(const char *path) {
    struct stat st;

    if (stat(path, &st) == -1) {
        perror("stat");
        return;
    }

    printf("%s\n", path);

    if (!S_ISDIR(st.st_mode)) {
        return;
    }

    DIR *dir = opendir(path);
    if (dir == NULL) {
        perror("opendir");
        return;
    }

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char child_path[PATH_MAX];

        snprintf(child_path, sizeof(child_path), "%s/%s", path, entry->d_name);

        recursive_find(child_path);
    }

    closedir(dir);
}

int main(int argc, char *argv[]) {
    const char *start_path;

    if (argc == 1) {
        start_path = ".";
    } else if (argc == 2) {
        start_path = argv[1];
    } else {
        fprintf(stderr, "Usage: %s [directory]\n", argv[0]);
        exit(1);
    }

    recursive_find(start_path);

    return 0;
}
~~~

---

## 4.3 编译运行

~~~bash
gcc myfind.c -o myfind

./myfind
./myfind test
~~~

---

## 4.4 核心思路

这个程序的核心是：

~~~c
void recursive_find(const char *path)
~~~

逻辑：

~~~text
打印 path
如果 path 是普通文件，结束
如果 path 是目录，打开它
读取目录中每个子项
对每个子项继续调用 recursive_find()
~~~

也就是：

~~~text
遇到文件：打印
遇到目录：打印，并进入目录继续找
~~~

---

## 4.5 使用 stat 判断文件类型

~~~c
if (stat(path, &st) == -1) {
    perror("stat");
    return;
}
~~~

`stat()` 获取路径对应对象的信息。

然后：

~~~c
if (!S_ISDIR(st.st_mode)) {
    return;
}
~~~

判断它是不是目录。

如果不是目录，打印完就返回。

如果是目录，就需要继续打开目录并读取内容。

---

## 4.6 为什么先打印 path

~~~c
printf("%s\n", path);
~~~

题目要求打印目录树下所有文件和目录名。

所以无论 `path` 是文件还是目录，都要先打印。

例如：

~~~text
test
test/a.txt
test/sub
test/sub/c.txt
~~~

这里目录 `test` 和 `test/sub` 也要打印。

---

## 4.7 打开目录

~~~c
DIR *dir = opendir(path);
~~~

如果 `path` 是目录，就用 `opendir()` 打开它。

如果失败：

~~~c
if (dir == NULL) {
    perror("opendir");
    return;
}
~~~

可能原因：

- 没有权限；
- 路径不存在；
- 文件系统错误。

---

## 4.8 读取目录项

~~~c
while ((entry = readdir(dir)) != NULL) {
    ...
}
~~~

`readdir()` 每次返回一个目录项。

目录项中最重要的是：

~~~c
entry->d_name
~~~

也就是文件名。

---

## 4.9 为什么要跳过 `.` 和 `..`

代码：

~~~c
if (strcmp(entry->d_name, ".") == 0 ||
    strcmp(entry->d_name, "..") == 0) {
    continue;
}
~~~

每个目录都包含：

| 目录项 | 含义 |
|---|---|
| `.` | 当前目录 |
| `..` | 父目录 |

如果不跳过它们，会无限递归。

例如：

~~~text
test/.
test/./.
test/././.
...
~~~

或者：

~~~text
test/..
test/../..
...
~~~

所以递归遍历目录时，必须跳过 `.` 和 `..`。

---

## 4.10 拼接子路径

~~~c
snprintf(child_path, sizeof(child_path), "%s/%s", path, entry->d_name);
~~~

`readdir()` 只返回名字，例如：

~~~text
a.txt
~~~

但递归调用需要完整路径，例如：

~~~text
test/a.txt
~~~

所以要把：

~~~text
path = "test"
entry->d_name = "a.txt"
~~~

拼成：

~~~text
child_path = "test/a.txt"
~~~

---

## 4.11 递归调用

~~~c
recursive_find(child_path);
~~~

这是整个程序最关键的地方。

意思是：

~~~text
发现一个子项 child_path
让 recursive_find() 继续处理它
如果它是文件，打印后返回
如果它是目录，继续打开并遍历
~~~

例如目录结构：

~~~text
test/
├── a.txt
└── sub/
    └── c.txt
~~~

调用过程大致是：

~~~text
recursive_find("test")
    打印 test
    发现 test/a.txt
        recursive_find("test/a.txt")
            打印 test/a.txt
            不是目录，返回
    发现 test/sub
        recursive_find("test/sub")
            打印 test/sub
            发现 test/sub/c.txt
                recursive_find("test/sub/c.txt")
                    打印 test/sub/c.txt
                    不是目录，返回
~~~

---

## 4.12 关闭目录

~~~c
closedir(dir);
~~~

打开目录会占用系统资源。

读取完目录后，要关闭目录。

---

## 4.13 main 函数参数处理

~~~c
if (argc == 1) {
    start_path = ".";
} else if (argc == 2) {
    start_path = argv[1];
} else {
    fprintf(stderr, "Usage: %s [directory]\n", argv[0]);
    exit(1);
}
~~~

如果没有传参数：

~~~bash
./myfind
~~~

则：

~~~c
start_path = ".";
~~~

从当前目录开始。

如果传了一个参数：

~~~bash
./myfind test
~~~

则：

~~~c
start_path = "test";
~~~

从 `test` 目录开始。

---

## 4.14 运行示例

创建测试目录：

~~~bash
mkdir -p test/sub/sub2
touch test/a.txt
touch test/b.txt
touch test/sub/c.txt
touch test/sub/sub2/d.txt
~~~

运行：

~~~bash
./myfind test
~~~

可能输出：

~~~text
test
test/a.txt
test/b.txt
test/sub
test/sub/c.txt
test/sub/sub2
test/sub/sub2/d.txt
~~~

注意：

~~~text
输出顺序取决于文件系统中目录项的读取顺序，不一定和示例完全一样。
~~~

---

# 5. 四道题核心 API 总结

## 5.1 `stat()`

~~~c
int stat(const char *pathname, struct stat *buf);
~~~

作用：

~~~text
获取文件或目录的元数据。
~~~

常用字段：

~~~c
st.st_ino
st.st_size
st.st_blocks
st.st_blksize
st.st_nlink
st.st_mode
st.st_uid
st.st_gid
st.st_mtime
~~~

---

## 5.2 `opendir()`

~~~c
DIR *opendir(const char *name);
~~~

作用：

~~~text
打开目录，返回目录流。
~~~

---

## 5.3 `readdir()`

~~~c
struct dirent *readdir(DIR *dirp);
~~~

作用：

~~~text
每次读取一个目录项。
~~~

目录项常用字段：

~~~c
entry->d_name
entry->d_ino
~~~

---

## 5.4 `closedir()`

~~~c
int closedir(DIR *dirp);
~~~

作用：

~~~text
关闭目录流。
~~~

---

## 5.5 `open()`

~~~c
int open(const char *pathname, int flags);
~~~

作用：

~~~text
打开文件，返回文件描述符。
~~~

例如：

~~~c
int fd = open(filename, O_RDONLY);
~~~

---

## 5.6 `read()`

~~~c
ssize_t read(int fd, void *buf, size_t count);
~~~

作用：

~~~text
从文件描述符 fd 读取数据到缓冲区 buf。
~~~

返回值：

| 返回值 | 含义 |
|---|---|
| `> 0` | 实际读到的字节数 |
| `= 0` | EOF |
| `< 0` | 错误 |

---

## 5.7 `write()`

~~~c
ssize_t write(int fd, const void *buf, size_t count);
~~~

作用：

~~~text
把缓冲区中的数据写入文件描述符。
~~~

打印到屏幕时常用：

~~~c
write(STDOUT_FILENO, buf, bytes_read);
~~~

---

## 5.8 `lseek()`

~~~c
off_t lseek(int fd, off_t offset, int whence);
~~~

作用：

~~~text
修改文件当前偏移量。
~~~

常用：

~~~c
lseek(fd, pos, SEEK_SET);
~~~

表示跳到文件偏移 `pos` 处。

---

## 5.9 `close()`

~~~c
int close(int fd);
~~~

作用：

~~~text
关闭文件描述符。
~~~

---

## 5.10 `getcwd()`

~~~c
char *getcwd(char *buf, size_t size);
~~~

作用：

~~~text
获取当前工作目录。
~~~

例如：

~~~c
getcwd(dirpath, sizeof(dirpath));
~~~

---

# 6. 四道题的核心模式

## 6.1 `mystat`

~~~text
stat(path)
    ↓
打印 inode、size、blocks、nlink、mode 等信息
~~~

---

## 6.2 `myls`

~~~text
opendir(dir)
    ↓
readdir() 逐个读取目录项
    ↓
普通模式：打印文件名
-l 模式：对每个文件 stat()，打印详细信息
    ↓
closedir()
~~~

---

## 6.3 `mytail`

~~~text
open(file)
    ↓
stat(file) 获取文件大小
    ↓
lseek 从文件末尾向前扫描
    ↓
找到最后 n 行起点
    ↓
read() 从该位置读到 EOF
    ↓
write() 输出
    ↓
close()
~~~

---

## 6.4 `myfind`

~~~text
recursive_find(path):
    stat(path)
    打印 path
    如果不是目录：返回
    如果是目录：
        opendir(path)
        readdir() 遍历子项
        跳过 . 和 ..
        拼接 child_path
        recursive_find(child_path)
        closedir()
~~~

---

# 7. 易错点

## 7.1 `stat()` 需要路径

`readdir()` 返回的只是文件名。

如果正在遍历指定目录，例如：

~~~bash
./myls -l testdir
~~~

读到：

~~~text
a.txt
~~~

调用 `stat()` 时不能直接：

~~~c
stat("a.txt", &st);
~~~

应该拼接：

~~~c
stat("testdir/a.txt", &st);
~~~

---

## 7.2 C 字符串必须以 `'\0'` 结尾

使用 `strncpy()` 后，最好手动保证结尾：

~~~c
dirpath[sizeof(dirpath) - 1] = '\0';
~~~

否则如果源字符串过长，可能没有字符串结束符。

---

## 7.3 递归遍历必须跳过 `.` 和 `..`

否则会无限递归。

必须写：

~~~c
if (strcmp(entry->d_name, ".") == 0 ||
    strcmp(entry->d_name, "..") == 0) {
    continue;
}
~~~

---

## 7.4 `read()` 不一定一次读完

所以输出文件剩余内容时要用循环：

~~~c
while ((bytes_read = read(fd, buf, BUF_SIZE)) > 0) {
    write(STDOUT_FILENO, buf, bytes_read);
}
~~~

---

## 7.5 `tail` 中最后一个换行符要特殊处理

如果文件最后一个字符是 `\n`，从后往前扫描时第一次遇到它，不应立即算作一整行边界。

所以代码中有：

~~~c
if (pos != file_size - 1) {
    lines++;
}
~~~

---

## 7.6 `lseek()` 只是改变偏移量

`lseek()` 不读数据，也不写数据。

它只是让下一次 `read()` 或 `write()` 从指定位置开始。

---

## 7.7 `ls -l` 的详细信息来自 inode

目录项主要包含：

~~~text
文件名
inode 号
~~~

文件大小、权限、所有者、修改时间等详细信息需要通过 `stat()` 获取。

---

# 8. 总结

这 4 道题对应了文件系统 API 的 4 类常见操作：

| 作业 | 功能 | 核心接口 |
|---|---|---|
| `mystat` | 查看文件元数据 | `stat()` |
| `myls` | 列出目录内容 | `opendir()`、`readdir()`、`stat()` |
| `mytail` | 输出文件末尾几行 | `open()`、`stat()`、`lseek()`、`read()` |
| `myfind` | 递归遍历目录树 | `stat()`、`opendir()`、`readdir()`、递归 |

最核心的一句话：

~~~text
目录遍历靠 opendir/readdir，文件属性靠 stat，文件内容靠 open/read/write，随机定位靠 lseek。
~~~