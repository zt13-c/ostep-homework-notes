# 操作系统文件系统 API 与目录机制笔记

## 1. 从进程、地址空间到持久存储

前面操作系统已经提供了两个重要抽象：

| 抽象 | 含义 |
|---|---|
| 进程 | 虚拟化 CPU |
| 地址空间 | 虚拟化内存 |

也就是说：

- 每个进程好像独占 CPU；
- 每个进程好像有自己的私有内存；
- 实际上 CPU 和内存都由操作系统统一管理和共享。

接下来引入第三个重要抽象：

> 持久存储 persistent storage

持久存储设备包括：

- 硬盘 HDD；
- 固态硬盘 SSD；
- U 盘；
- 光盘；
- 其他长期保存数据的设备。

内存断电后数据会丢失，而持久存储设备断电后数据仍然保留。

因此文件系统要解决的核心问题是：

> 操作系统如何管理持久存储设备，使用户能够方便、可靠、高效地保存和访问数据？

---

## 2. 文件和目录的基本抽象

### 2.1 文件 file

文件可以理解为：

> 一个线性的字节数组。

例如文件内容可能是：

~~~text
h e l l o \n
~~~

文件系统通常不关心这些字节到底表示什么。

它不关心：

- 这是文本；
- 这是图片；
- 这是 C 代码；
- 这是音频文件。

它主要关心：

- 这些字节存在哪里；
- 文件大小是多少；
- 如何读出来；
- 如何写进去；
- 如何保证持久化。

### 2.2 inode 号

每个文件在文件系统内部都有一个低级名称，通常叫：

> inode number

可以理解为：

| 名称 | 作用 |
|---|---|
| 文件名 | 给用户看的名字 |
| inode 号 | 文件系统内部真正识别文件的编号 |

例如：

~~~text
file -> inode 67158084
~~~

文件名只是目录中的一个名字，真正描述文件的是 inode。

### 2.3 目录 directory

目录也是一种特殊文件。

普通文件保存的是普通字节内容，而目录保存的是：

> 文件名到 inode 号的映射关系。

例如某个目录中可能保存：

~~~text
a.txt  -> inode 101
b.txt  -> inode 102
foo    -> inode 103
~~~

所以目录的本质是：

> 一个由文件系统维护的“名字表”。

---

## 3. 路径 pathname

### 3.1 路径是什么

路径不是文件，也不是目录本身。

路径是：

> 用来定位某个文件系统对象的一串名字。

例如：

~~~bash
~/foo/a.txt
~~~

这是路径。

它表示：

1. 从用户家目录 `~` 出发；
2. 找到 `foo`；
3. 再在 `foo` 中找到 `a.txt`。

如果 `foo` 是目录，那么 `~/foo` 这个路径指向一个目录。

如果 `foo` 是普通文件，那么 `~/foo` 这个路径指向一个普通文件。

所以要区分：

| 表达 | 含义 |
|---|---|
| `foo` | 一个名字 |
| `~/foo` | 一个路径 |
| `~/foo` 指向的对象 | 可能是目录，也可能是文件 |

### 3.2 绝对路径

从根目录 `/` 开始的路径叫绝对路径。

例如：

~~~bash
/foo/bar.txt
/bar/foo/bar.txt
~~~

它们都从根目录 `/` 出发。

### 3.3 路径查找过程

例如访问：

~~~bash
/home/czt/foo/a.txt
~~~

操作系统会：

~~~text
从根目录 / 开始
  ↓
在 / 中查找 home
  ↓
在 /home 中查找 czt
  ↓
在 /home/czt 中查找 foo
  ↓
在 /home/czt/foo 中查找 a.txt
~~~

路径查找的本质是：

> 一层一层读取目录中的 “名字 -> inode” 映射。

---

## 4. 创建文件：open()

### 4.1 使用 open 创建文件

创建文件可以通过 `open()` 加 `O_CREAT` 标志完成。

例如：

~~~c
int fd = open("foo", O_CREAT | O_WRONLY | O_TRUNC);
~~~

含义是：

- 打开当前目录下的 `foo`；
- 如果不存在，就创建；
- 以只写方式打开；
- 如果已经存在，就清空原内容；
- 返回文件描述符 `fd`。

### 4.2 标志解释

#### `O_CREAT`

表示：

> 如果文件不存在，就创建它。

#### `O_WRONLY`

表示：

> 以只写方式打开。

打开后可以：

~~~c
write(fd, buffer, size);
~~~

但不能用这个 `fd` 读取。

#### `O_TRUNC`

表示：

> 如果文件已经存在，就把原有内容清空。

例如原文件内容是：

~~~text
hello world
~~~

执行：

~~~c
open("foo", O_CREAT | O_WRONLY | O_TRUNC);
~~~

之后，`foo` 仍然存在，但内容变为空。

### 4.3 creat()

早期 UNIX 也提供了：

~~~c
int fd = creat("foo");
~~~

它大致等价于：

~~~c
open("foo", O_CREAT | O_WRONLY | O_TRUNC);
~~~

后来 `open()` 通过不同标志可以完成更多功能，因此更通用。

---

## 5. 文件描述符 file descriptor

### 5.1 文件描述符是什么

`open()` 成功后返回一个整数，叫：

> 文件描述符 file descriptor

例如：

~~~c
int fd = open("foo", O_WRONLY);
~~~

如果返回 `3`，表示当前进程可以通过 `fd=3` 操作这个已打开文件。

文件描述符不是 inode 号，也不是文件内容，而是当前进程访问已打开文件的句柄。

### 5.2 默认文件描述符

每个进程启动时通常已经打开了三个文件描述符：

| fd | 名称 | 含义 |
|---|---|---|
| 0 | stdin | 标准输入 |
| 1 | stdout | 标准输出 |
| 2 | stderr | 标准错误 |

所以程序第一次再打开普通文件时，通常得到：

~~~text
fd = 3
~~~

### 5.3 每个进程有自己的文件描述符表

例如：

~~~text
进程 A 的 fd 表：

0 -> stdin
1 -> stdout
2 -> stderr
3 -> foo

进程 B 的 fd 表：

0 -> stdin
1 -> stdout
2 -> stderr
3 -> bar
~~~

所以不同进程中的 `fd=3` 不一定表示同一个文件。

### 5.4 fork 后文件描述符是否共享

如果父进程先 `open()`，再 `fork()`：

~~~c
int fd = open("foo", O_RDONLY);
fork();
~~~

那么：

- 父子进程各自有一张文件描述符表；
- 但是复制出来的 fd 条目指向同一个打开文件对象；
- 因此共享文件偏移量 offset。

可以理解为：

~~~text
父进程 fd 表                    子进程 fd 表
+------+---------+              +------+---------+
|  3   |   ----  |              |  3   |   ----  |
+------+----|----+              +------+----|----+
            |                          |
            +-----------+--------------+
                        |
                        v
              同一个打开文件对象
              offset、flags 等状态
                        |
                        v
                    inode / 文件 foo
~~~

如果父进程读了 10 字节，offset 增加 10，子进程再读会从新的 offset 开始。

但如果父子进程分别独立 `open()` 同一个文件，则它们有不同的打开文件对象，offset 不共享。

总结：

~~~text
open 后 fork：共享打开文件对象，offset 共享。
fork 后各自 open：打开文件对象不同，offset 不共享。
~~~

---

## 6. 读写文件：read() 和 write()

### 6.1 cat foo 背后的系统调用

命令：

~~~bash
echo hello > foo
cat foo
~~~

`cat foo` 背后大致会执行：

~~~c
open("foo", O_RDONLY | O_LARGEFILE) = 3
read(3, "hello\n", 4096)            = 6
write(1, "hello\n", 6)              = 6
read(3, "", 4096)                  = 0
close(3)                           = 0
~~~

### 6.2 open

~~~c
open("foo", O_RDONLY | O_LARGEFILE) = 3
~~~

表示：

- 以只读方式打开 `foo`；
- 返回文件描述符 `3`。

### 6.3 read

~~~c
read(3, "hello\n", 4096) = 6
~~~

真实代码更像：

~~~c
char buf[4096];
int n = read(3, buf, 4096);
~~~

含义是：

- 从 `fd=3` 对应的文件中读；
- 最多读 4096 字节；
- 读到用户缓冲区 `buf` 中；
- 实际读到 6 字节。

这里 `strace` 显示的 `"hello\n"` 不是说源码里传入了字符串，而是它把缓冲区中读到的内容展示出来了。

所以：

~~~text
read(3, "hello\n", 4096) = 6
~~~

应理解为：

~~~text
read(3, buf, 4096) = 6
执行后 buf 中的内容是 "hello\n"
~~~

### 6.4 write

~~~c
write(1, "hello\n", 6) = 6
~~~

表示：

- 把缓冲区中的 6 字节写到 `fd=1`；
- `fd=1` 是标准输出；
- 所以内容显示到屏幕上。

### 6.5 EOF

~~~c
read(3, "", 4096) = 0
~~~

`read()` 返回 0 表示：

> 已经读到文件末尾 EOF。

### 6.6 close

~~~c
close(3) = 0
~~~

表示关闭文件描述符 `3`。

---

## 7. 缓冲区 buffer

### 7.1 缓冲区是什么

缓冲区就是：

> 内存中临时存放数据的一块区域。

例如：

~~~c
char buf[4096];
~~~

`read()` 需要一个缓冲区，用来存放从文件读出的数据。

### 7.2 read 的函数形式

~~~c
ssize_t read(int fd, void *buf, size_t count);
~~~

三个参数：

| 参数 | 含义 |
|---|---|
| `fd` | 从哪个文件描述符读 |
| `buf` | 读到哪块内存 |
| `count` | 最多读多少字节 |

### 7.3 read 和 I/O 的关系

`read()` 是系统调用。

I/O 是更大的概念，表示输入输出操作，例如：

- 从文件读数据；
- 向文件写数据；
- 从键盘读输入；
- 向屏幕输出；
- 网络收发数据；
- 磁盘读写块。

`read()` 是发起输入类 I/O 的接口。

但调用 `read()` 不一定真的访问磁盘。

如果数据已经在内核页缓存中，那么只需从内核缓存复制到用户缓冲区。

如果数据不在缓存中，才需要实际磁盘 I/O。

流程可能是：

~~~text
磁盘
  ↓
内核页缓存
  ↓
用户缓冲区 buf
  ↓
程序使用
~~~

或者如果已有缓存：

~~~text
内核页缓存
  ↓
用户缓冲区 buf
~~~

---

## 8. 文件偏移量和 lseek()

### 8.1 顺序读写

普通 `read()` 和 `write()` 默认从当前文件偏移量开始。

例如文件内容：

~~~text
abcdef
~~~

第一次：

~~~c
read(fd, buf, 2);
~~~

读到：

~~~text
ab
~~~

offset 从 0 变成 2。

第二次再读 2 字节，会读到：

~~~text
cd
~~~

### 8.2 lseek 的作用

`lseek()` 用来修改当前文件偏移量。

函数原型：

~~~c
off_t lseek(int fd, off_t offset, int whence);
~~~

它不读数据，也不写数据，只改变下一次读写的位置。

### 8.3 whence 的三种方式

#### SEEK_SET

从文件开头计算：

~~~c
lseek(fd, 100, SEEK_SET);
~~~

表示把 offset 设置为 100。

#### SEEK_CUR

从当前位置计算：

~~~c
lseek(fd, 10, SEEK_CUR);
~~~

表示当前位置向后移动 10 字节。

也可以用负数向前移动：

~~~c
lseek(fd, -5, SEEK_CUR);
~~~

#### SEEK_END

从文件末尾计算：

~~~c
lseek(fd, 0, SEEK_END);
~~~

表示移动到文件末尾。

~~~c
lseek(fd, -10, SEEK_END);
~~~

表示移动到文件末尾前 10 字节的位置。

### 8.4 lseek 不会执行磁盘寻道

`lseek()` 只是修改内核中记录的文件逻辑偏移量。

它不会马上移动磁盘磁头，也不会自动触发磁盘 I/O。

真正的 I/O 是后续的：

~~~c
read(fd, buf, size);
write(fd, buf, size);
~~~

才可能触发。

一句话：

~~~text
lseek 移动的是文件的逻辑读写位置，不是直接移动磁盘磁头。
~~~

---

## 9. fsync()：强制立即写入

### 9.1 write 不等于马上落盘

调用：

~~~c
write(fd, buffer, size);
~~~

通常只是把数据写到内核缓存中，不一定马上写到磁盘。

流程是：

~~~text
用户程序 buffer
    ↓ write()
内核页缓存 / buffer cache
    ↓ 稍后写回
磁盘 / SSD
~~~

### 9.2 dirty data

脏数据 dirty data 指：

> 已经在内存中被修改，但还没有写回磁盘的数据。

此时：

~~~text
内存中：新版本
磁盘上：旧版本
~~~

如果系统在写回前崩溃，数据可能丢失。

### 9.3 fsync 的作用

函数：

~~~c
int fsync(int fd);
~~~

作用是：

> 强制把 fd 对应文件的脏数据写到持久存储设备上。

示例：

~~~c
int fd = open("foo", O_CREAT | O_WRONLY | O_TRUNC);
assert(fd > -1);

int rc = write(fd, buffer, size);
assert(rc == size);

rc = fsync(fd);
assert(rc == 0);
~~~

含义：

- `write()` 把数据交给内核；
- `fsync()` 强制文件数据落盘；
- `fsync()` 返回后，程序可以更有把握认为数据已经持久化。

### 9.4 fsync 文件还不一定够

如果是新建文件，除了文件内容，还修改了所在目录的目录项：

~~~text
foo -> inode
~~~

因此严格来说，可能还需要对包含该文件的目录执行 `fsync()`，保证目录项也落盘。

总结：

~~~text
write 成功 ≠ 数据已落盘
fsync 文件 = 保证文件内容落盘
fsync 目录 = 保证名字到 inode 的映射落盘
~~~

---

## 10. 文件重命名 rename()

### 10.1 mv 背后的系统调用

命令：

~~~bash
mv foo bar
~~~

通常使用系统调用：

~~~c
rename("foo", "bar");
~~~

它把旧名字 `foo` 改成新名字 `bar`。

### 10.2 rename 的原子性

`rename()` 通常是原子的。

原子性表示：

> 要么完整发生，要么完全不发生，不会出现中间状态。

例如：

~~~c
rename("foo", "bar");
~~~

系统崩溃后通常只可能是：

- 仍然叫 `foo`；
- 已经叫 `bar`。

不会出现：

- `foo` 没了；
- `bar` 也没有；
- 半改名状态。

### 10.3 安全更新文件

直接覆盖旧文件有风险。

例如：

~~~c
open("foo.txt", O_WRONLY | O_TRUNC);
write(fd, new_buffer, size);
~~~

如果系统在中间崩溃，可能导致：

- 旧内容被清空；
- 新内容只写了一半；
- 文件损坏。

更安全的方式是：

~~~c
int fd = open("foo.txt.tmp", O_WRONLY | O_CREAT | O_TRUNC);
write(fd, buffer, size);
fsync(fd);
close(fd);
rename("foo.txt.tmp", "foo.txt");
~~~

流程：

1. 写入临时文件；
2. `fsync()` 临时文件；
3. 使用 `rename()` 原子替换旧文件。

这样崩溃后通常只会看到：

- 旧版本；
- 新版本。

不会看到半旧半新的文件。

严格情况下，`rename()` 后还应 `fsync()` 所在目录。

---

## 11. 获取文件信息：stat()

### 11.1 文件元数据 metadata

文件系统不仅保存文件内容，还保存文件元数据。

元数据包括：

- 文件大小；
- inode 号；
- 权限；
- 所有者；
- 所属组；
- 硬链接数量；
- 时间戳；
- 占用块数；
- 文件类型。

### 11.2 stat 系统调用

函数形式：

~~~c
int stat(const char *pathname, struct stat *buf);
~~~

作用是：

> 根据路径找到文件，并把文件元数据填入 `struct stat`。

### 11.3 struct stat

常见字段：

~~~c
struct stat {
    dev_t     st_dev;     /* device ID */
    ino_t     st_ino;     /* inode number */
    mode_t    st_mode;    /* protection and file type */
    nlink_t   st_nlink;   /* number of hard links */
    uid_t     st_uid;     /* user ID of owner */
    gid_t     st_gid;     /* group ID of owner */
    off_t     st_size;    /* total size, in bytes */
    blksize_t st_blksize; /* blocksize for filesystem I/O */
    blkcnt_t  st_blocks;  /* number of blocks allocated */
    time_t    st_atime;   /* time of last access */
    time_t    st_mtime;   /* time of last modification */
    time_t    st_ctime;   /* time of last status change */
};
~~~

### 11.4 三个时间

| 字段 | 含义 |
|---|---|
| `st_atime` | 最后访问时间 |
| `st_mtime` | 文件内容最后修改时间 |
| `st_ctime` | inode 状态最后改变时间 |

注意：

> `ctime` 不是 creation time，而是 change time。

例如：

~~~bash
chmod 600 file
~~~

会改变权限，因此更新 `ctime`，但不一定更新 `mtime`。

### 11.5 inode 保存什么

inode 通常保存：

- 文件类型；
- 权限；
- 所有者；
- 文件大小；
- 时间戳；
- 硬链接数量；
- 数据块位置。

但文件名一般不在 inode 中，而是在目录中。

关系是：

~~~text
目录：
    name -> inode number

inode：
    metadata + block pointers

数据块：
    actual bytes
~~~

一句话：

> 目录负责按名字找到 inode，inode 负责描述文件并找到数据。

---

## 12. 创建目录 mkdir()

### 12.1 目录也是特殊文件

目录保存的是：

~~~text
文件名 -> inode 号
~~~

因为目录内容是文件系统关键元数据，所以不能像普通文件一样随便 `write()`。

目录修改必须通过系统调用完成，例如：

- `mkdir()`；
- `rmdir()`；
- `link()`；
- `unlink()`；
- `rename()`。

### 12.2 mkdir

创建目录：

~~~c
mkdir("foo", 0777);
~~~

命令行：

~~~bash
mkdir foo
~~~

`strace` 可能看到：

~~~c
mkdir("foo", 0777) = 0
~~~

含义：

- 创建名为 `foo` 的目录；
- 权限参数是 `0777`；
- 返回 0 表示成功。

实际权限还会受到 `umask` 影响。

### 12.3 新目录不是真的空

新目录至少包含两个特殊目录项：

| 目录项 | 含义 |
|---|---|
| `.` | 当前目录自己 |
| `..` | 父目录 |

例如：

~~~text
foo/
├── .   -> foo 自己
└── ..  -> foo 的父目录
~~~

### 12.4 mkdir 后底层发生什么

假设当前目录 `/home/czt` 的 inode 是 50。

执行：

~~~bash
mkdir foo
~~~

文件系统大概做：

1. 分配一个新 inode，例如 100，类型为 directory；
2. 分配数据块保存目录项；
3. 在 `foo` 自己的数据块中写入：

~~~text
.   -> 100
..  -> 50
~~~

4. 在父目录 `/home/czt` 的目录数据块中加入：

~~~text
foo -> 100
~~~

所以图形界面中看到的“文件夹”，底层就是一个类型为 directory 的 inode 和它的数据块。

---

## 13. 读取目录 opendir/readdir/closedir

### 13.1 目录不能用普通 read 读取

普通文件可以：

~~~c
open();
read();
close();
~~~

目录需要使用专门接口：

~~~c
opendir();
readdir();
closedir();
~~~

### 13.2 示例程序

~~~c
int main(int argc, char *argv[]) {
    DIR *dp = opendir(".");
    assert(dp != NULL);

    struct dirent *d;

    while ((d = readdir(dp)) != NULL) {
        printf("%d %s\n", (int) d->d_ino, d->d_name);
    }

    closedir(dp);
    return 0;
}
~~~

功能：

- 打开当前目录；
- 一项一项读取目录项；
- 打印 inode 号和文件名；
- 关闭目录。

这是一个简化版 `ls`。

### 13.3 struct dirent

目录项结构大致包含：

~~~c
struct dirent {
    char           d_name[256]; /* filename */
    ino_t          d_ino;       /* inode number */
    off_t          d_off;       /* offset to next dirent */
    unsigned short d_reclen;    /* length of this record */
    unsigned char  d_type;      /* type of file */
};
~~~

最重要的是：

| 字段 | 含义 |
|---|---|
| `d_name` | 文件名 |
| `d_ino` | inode 号 |

例如目录内部有：

~~~text
.       -> 100
..      -> 50
a.txt   -> 101
b.txt   -> 102
~~~

`readdir()` 每次返回一条目录项。

### 13.4 ls 和 ls -l

普通 `ls` 只需要知道目录里有哪些名字，主要使用：

~~~text
opendir()
readdir()
closedir()
~~~

而 `ls -l` 需要显示：

- 权限；
- 所有者；
- 文件大小；
- 修改时间；
- 硬链接数。

这些详细信息在 inode 中，所以还需要对每个文件调用：

~~~c
stat()
~~~

总结：

~~~text
readdir() 负责看目录里有哪些名字。
stat() 负责看这些名字对应文件的详细属性。
~~~

---

## 14. 目录的直观理解

### 14.1 目录文件存在哪里

目录文件和普通文件一样，存储在磁盘或 SSD 的文件系统中。

普通文件：

~~~text
inode
  └── 数据块
        └── hello world
~~~

目录文件：

~~~text
inode
  └── 数据块
        └── name -> inode 映射表
~~~

### 14.2 foo 是目录吗

如果执行：

~~~bash
mkdir foo
~~~

那么：

- `foo` 是父目录中的一个名字；
- 这个名字指向某个 inode；
- 该 inode 的类型是 directory；
- 所以通常说 `foo` 是一个目录。

### 14.3 touch 文件时目录如何变化

执行：

~~~bash
touch ~/foo/a.txt
~~~

如果 `~/foo` 是目录，那么会在 `foo` 目录的数据块中增加一条：

~~~text
a.txt -> inode 101
~~~

所以映射关系存在：

> `foo` 这个目录文件自己的数据块里。

它不是普通意义上的隐藏文件，而是文件系统内部格式，不能直接像文本一样编辑，只能通过系统调用和命令间接修改。

---

## 15. 递归删除

递归删除表示：

> 删除目录时，不只删除目录本身，还会进入它内部，把所有文件、子目录、子目录中的内容全部删除。

例如目录：

~~~text
project/
├── main.c
├── README.md
├── data/
│   ├── a.txt
│   └── b.txt
└── build/
    └── output/
        └── result.txt
~~~

执行：

~~~bash
rm -r project
~~~

会删除：

~~~text
project/main.c
project/README.md
project/data/a.txt
project/data/b.txt
project/data/
project/build/output/result.txt
project/build/output/
project/build/
project/
~~~

`-r` 表示 recursive，递归。

更危险的是：

~~~bash
rm -rf *
~~~

含义：

- `-r`：递归删除；
- `-f`：强制删除，不提示；
- `*`：当前目录下所有非隐藏项。

执行前一定要确认：

~~~bash
pwd
ls
~~~

---

## 16. 硬链接 hard link

### 16.1 核心思想

硬链接就是：

> 给同一个 inode 增加另一个名字。

例如：

~~~bash
echo hello > file
ln file file2
~~~

原来目录中：

~~~text
file -> inode 67158084
~~~

执行硬链接后：

~~~text
file  -> inode 67158084
file2 -> inode 67158084
~~~

所以 `file` 和 `file2` 是两个名字，但指向同一个 inode。

### 16.2 inode 相同

命令：

~~~bash
ls -i file file2
~~~

可能输出：

~~~text
67158084 file
67158084 file2
~~~

说明两个名字指向同一个 inode。

### 16.3 没有原文件和链接文件之分

硬链接创建后：

~~~text
file 和 file2 地位相同。
~~~

不是：

~~~text
file 是真文件，file2 是快捷方式。
~~~

而是：

~~~text
file 和 file2 都是 inode 67158084 的名字。
~~~

### 16.4 修改一个，另一个也变

因为它们是同一份数据。

例如：

~~~bash
echo world > file
cat file2
~~~

会看到：

~~~text
world
~~~

### 16.5 unlink 和 link count

删除文件命令：

~~~bash
rm file
~~~

底层通常调用：

~~~c
unlink("file");
~~~

`unlink()` 的本质是：

> 删除目录中某个名字到 inode 的链接。

如果原来：

~~~text
file  -> inode 67158084
file2 -> inode 67158084
~~~

执行：

~~~bash
rm file
~~~

之后：

~~~text
file2 -> inode 67158084
~~~

数据仍然存在。

inode 中有 link count，表示有多少个目录项指向它。

当 link count 变成 0，并且没有进程还打开该文件时，文件系统才真正释放 inode 和数据块。

### 16.6 硬链接和复制的区别

复制：

~~~bash
cp file file2
~~~

结果：

~~~text
file  -> inode 100 -> 数据 hello
file2 -> inode 200 -> 数据 hello
~~~

两个文件之后互不影响。

硬链接：

~~~bash
ln file file2
~~~

结果：

~~~text
file  -> inode 100
file2 -> inode 100
~~~

两个名字访问同一份数据。

---

## 17. 符号链接 symbolic link / soft link

### 17.1 创建符号链接

命令：

~~~bash
ln -s file file2
~~~

表示创建一个符号链接 `file2`，指向 `file`。

### 17.2 符号链接的本质

符号链接不是直接指向目标 inode，而是一个新的特殊文件。

例如：

~~~text
file  -> inode 100，普通文件，内容是 hello

file2 -> inode 200，符号链接文件，内容是字符串 "file"
~~~

所以符号链接本质是：

> 一个保存目标路径名的特殊文件。

### 17.3 为什么 cat file2 能读到 file

访问：

~~~bash
cat file2
~~~

系统发现 `file2` 是符号链接，于是读取其中保存的路径：

~~~text
file
~~~

然后继续按这个路径查找真正文件。

过程：

~~~text
file2
  ↓ 符号链接内容是 "file"
file
  ↓
inode 100
  ↓
数据 hello
~~~

### 17.4 ls -al 中的符号链接

可能看到：

~~~text
-rw-r----- 1 remzi remzi 6 May 3 19:10 file
lrwxrwxrwx 1 remzi remzi 4 May 3 19:10 file2 -> file
~~~

第一列：

- `-` 表示普通文件；
- `l` 表示符号链接。

`file2 -> file` 表示：

> file2 是符号链接，指向路径 file。

### 17.5 符号链接大小

如果：

~~~bash
ln -s file file2
~~~

`file2` 的大小可能是 4，因为它保存的字符串是：

~~~text
file
~~~

长度为 4 字节。

如果目标路径更长，符号链接文件也更大。

### 17.6 符号链接的优点

符号链接可以：

- 指向目录；
- 跨文件系统；
- 指向尚不存在的路径。

硬链接通常不能跨文件系统，也通常不能给目录创建硬链接。

### 17.7 悬空引用 dangling reference

如果：

~~~bash
echo hello > file
ln -s file file2
rm file
cat file2
~~~

会报错：

~~~text
cat: file2: No such file or directory
~~~

因为 `file2` 还存在，但它保存的路径 `file` 已经不存在了。

这种情况叫：

> 悬空引用 dangling reference  
> 断开的符号链接 broken symlink

### 17.8 硬链接与符号链接对比

| 对比 | 硬链接 | 符号链接 |
|---|---|---|
| 本质 | 多个名字指向同一个 inode | 一个特殊文件，内容是目标路径 |
| 是否有新 inode | 没有，为目标 inode 增加名字 | 有，符号链接本身有 inode |
| 删除原名字后 | 另一个名字仍可访问数据 | 可能变成悬空链接 |
| 是否可跨文件系统 | 通常不行 | 可以 |
| 是否可指向目录 | 通常不允许普通用户创建 | 可以 |
| 是否有原文件和链接之分 | 没有明显主次 | 有，链接指向目标路径 |

图示：

~~~text
硬链接：

目录
├── file  ──┐
└── file2 ──┤
            ↓
        inode 100
            ↓
          hello


符号链接：

目录
├── file  ─────→ inode 100
│                 ↓
│               hello
│
└── file2 ─────→ inode 200
                  ↓
              内容是字符串 "file"
                  ↓
              再去找 file
~~~

---

## 18. 为什么不允许目录硬链接

如果允许普通用户给目录创建硬链接，目录树可能形成环。

例如让：

~~~text
A/B 指向 A
~~~

则：

~~~text
A/B = A
A/B/B = A
A/B/B/B = A
...
~~~

路径可以无限写下去：

~~~text
A/B/B/B/B/B/...
~~~

这样目录结构不再是树，而是有环图。

递归遍历或递归删除可能无限循环：

~~~bash
find A
rm -r A
~~~

所以大多数 UNIX/Linux 系统不允许普通用户给目录创建硬链接。

目录中的 `.` 和 `..` 是文件系统自己维护的特殊例外。

---

## 19. 创建并挂载文件系统

### 19.1 mkfs：创建文件系统

一个磁盘分区刚开始可能只是原始块设备，例如：

~~~bash
/dev/sda1
~~~

要在上面创建文件系统，需要使用：

~~~bash
mkfs -t ext3 /dev/sda1
~~~

`mkfs` 表示 make file system。

它会在设备上创建文件系统所需结构，例如：

- 超级块 superblock；
- inode 区域；
- 数据块区域；
- 空闲块位图；
- 根目录。

注意：

> `mkfs` 类似格式化，通常会破坏原有数据。

### 19.2 mount：挂载文件系统

即使 `/dev/sda1` 上已经有文件系统，也需要挂载到当前目录树中才能访问。

命令：

~~~bash
mount -t ext3 /dev/sda1 /home/users
~~~

含义：

- 把 `/dev/sda1` 上的 ext3 文件系统；
- 挂载到当前系统目录树的 `/home/users`；
- `/home/users` 成为挂载点 mount point。

### 19.3 挂载点

挂载点就是：

> 当前目录树中的一个目录，用作另一个文件系统的入口。

例如 `/dev/sda1` 内部原来有：

~~~text
/
├── a
│   └── foo
└── b
    └── foo
~~~

挂载到 `/home/users` 后，系统中看到：

~~~text
/home/users/
├── a
│   └── foo
└── b
    └── foo
~~~

于是可以访问：

~~~bash
/home/users/a/foo
/home/users/b/foo
~~~

### 19.4 挂载会遮住原目录内容

如果挂载前 `/home/users` 中有：

~~~text
old.txt
~~~

挂载后访问 `/home/users`，看到的是被挂载文件系统的内容。

原来的 `old.txt` 没有删除，只是被遮住了。

卸载后：

~~~bash
umount /home/users
~~~

原内容又会重新可见。

### 19.5 多个文件系统统一成一棵树

Linux/UNIX 通过 mount 把多个文件系统统一到一棵目录树中。

例如：

~~~text
/              来自主硬盘根分区
/home          可能来自另一个分区
/mnt/usb       可能来自 U 盘
/proc          虚拟文件系统
/tmp           可能是 tmpfs
/afs           网络文件系统
~~~

用户只需要通过统一路径访问。

### 19.6 mount 输出示例

~~~text
/dev/sda1 on / type ext3 (rw)
proc on /proc type proc (rw)
sysfs on /sys type sysfs (rw)
/dev/sda5 on /tmp type ext3 (rw)
tmpfs on /dev/shm type tmpfs (rw)
AFS on /afs type afs (rw)
~~~

格式：

~~~text
设备或文件系统 on 挂载点 type 文件系统类型 (选项)
~~~

例如：

~~~text
/dev/sda1 on / type ext3 (rw)
~~~

表示：

- `/dev/sda1` 挂载到根目录 `/`；
- 文件系统类型是 ext3；
- `rw` 表示可读写。

### 19.7 mkfs 和 mount 的区别

| 命令 | 作用 |
|---|---|
| `mkfs` | 在设备上创建文件系统 |
| `mount` | 把已有文件系统接到当前目录树中 |

类比：

~~~text
mkfs：造一个仓库并建立货架规则。
mount：把仓库入口接到城市道路系统的某个路口。
~~~

---

## 20. 写时复制 Copy-on-Write 补充

### 20.1 fork 后虚拟内存共享

`fork()` 后，父子进程的虚拟地址空间看似相同。

为了避免立即复制整个进程内存，操作系统会让父子进程暂时共享同一批物理页，并把这些页标记为只读和 COW。

### 20.2 修改变量 x 时复制什么

如果子进程修改变量 `x`：

~~~c
x = 200;
~~~

会触发缺页异常。

操作系统发现这是写时复制，于是：

1. 分配一个新的物理页；
2. 把原物理页整页复制过去；
3. 修改子进程页表，让该虚拟页指向新物理页；
4. 将新页改为可写；
5. 重新执行写操作。

重点：

> 复制的是变量 `x` 所在的整页，不是整个进程，也不是 `x` 这几个字节。

常见页大小是 4KB。

所以：

~~~text
复制粒度 = 页 page
不是变量 variable
不是整个地址空间
~~~

如果 `x` 和 `y` 在同一页，写 `x` 时复制整页，之后写 `y` 不需要再次复制。

如果 `y` 在另一个页，写 `y` 会再触发一次 COW。

---

## 21. 总结

本章围绕 UNIX 文件系统 API 展开，核心内容如下：

1. 文件是字节数组；
2. 目录是特殊文件，保存 `名字 -> inode` 映射；
3. 路径是定位文件系统对象的名字序列；
4. `open()` 创建或打开文件，并返回文件描述符；
5. `read()` 和 `write()` 通过缓冲区完成输入输出；
6. `lseek()` 修改文件当前偏移量，不直接产生磁盘 I/O；
7. `write()` 不保证立即落盘，`fsync()` 用于强制持久化；
8. `rename()` 通常具有原子性，可用于安全文件更新；
9. `stat()` 获取文件元数据；
10. `mkdir()` 创建目录，新目录包含 `.` 和 `..`；
11. `readdir()` 读取目录项；
12. 硬链接是多个名字指向同一个 inode；
13. 符号链接是保存路径名的特殊文件；
14. `unlink()` 删除的是名字到 inode 的链接；
15. `mkfs()` 创建文件系统；
16. `mount()` 把文件系统接到统一目录树中。

---

## 22. 易错点

### 22.1 文件名不是文件本身

文件名只是目录项中的名字。

真正的文件由 inode 和数据块表示。

~~~text
文件名 -> inode -> 数据块
~~~

### 22.2 目录不是普通文本文件

目录虽然也是文件，但它是特殊文件。

里面保存的是文件系统内部格式的目录项，不能随便 `write()`。

### 22.3 路径不是目录

路径是定位对象的方法。

`~/foo` 是路径，它可以指向目录，也可以指向普通文件。

### 22.4 read 的第二个参数不是字符串

`strace` 中：

~~~c
read(3, "hello\n", 4096) = 6
~~~

表示缓冲区读到了 `"hello\n"`。

真实代码中第二个参数通常是：

~~~c
char buf[4096];
read(3, buf, 4096);
~~~

### 22.5 write 成功不代表数据落盘

`write()` 通常只是写到内核缓存。

如果需要持久化保证，要使用：

~~~c
fsync(fd);
~~~

### 22.6 lseek 不等于磁盘 seek

`lseek()` 只是修改文件逻辑偏移量。

不一定导致磁盘磁头移动。

### 22.7 硬链接不是快捷方式

硬链接是多个名字指向同一个 inode，没有原文件和链接文件的明显主次之分。

### 22.8 符号链接可能断开

符号链接保存的是路径。

如果目标路径不存在，符号链接会变成悬空引用。

### 22.9 rm 本质是 unlink

删除文件名只是解除目录项到 inode 的链接。

只有 link count 为 0 且没有进程打开时，数据才真正释放。

### 22.10 mount 会遮住原目录内容

把文件系统挂载到某目录后，该目录原来的内容不会消失，但会被暂时遮住。

---

## 23. 简短记忆版

~~~text
文件 = 字节数组
目录 = 名字到 inode 的映射表
inode = 文件元数据 + 数据块位置
路径 = 一层层目录查找的名字序列

open  -> 得到 fd
read  -> 从 fd 读到 buffer
write -> 从 buffer 写到 fd
lseek -> 改 offset
fsync -> 强制落盘
rename -> 原子改名
stat -> 查元数据
mkdir -> 创建目录
readdir -> 读目录项
link -> 创建硬链接
unlink -> 删除名字链接
ln -s -> 创建符号链接
mkfs -> 创建文件系统
mount -> 把文件系统接到目录树
~~~