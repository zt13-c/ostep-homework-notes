#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc,char*argv[])
{
    struct stat st;

    if(argc != 2)
    {
        fprintf(stderr,"Usage: %s <file-or-diretory>\n",argv[0]);
        exit(1);
    }

    if(stat(argv[1],&st) == -1)
    {
        perror("stat");
        exit(1);
    }

    printf("path: %s\n",argv[1]);
    printf("Inode: %lu\n", (unsigned long)st.st_ino);
    printf("Size: %lld bytes\n", (long long)st.st_size);
    printf("Blocks: %lld\n", (long long)st.st_blocks);
    printf("IO Block Size: %ld\n", (long)st.st_blksize);
    printf("Links: %lu\n", (unsigned long)st.st_nlink);
    printf("Mode: %o\n", st.st_mode);
    printf("UID: %u\n", st.st_uid);
    printf("GID: %u\n", st.st_gid);

    if(S_ISDIR(st.st_mode))
    {
        printf("Type: Directory\n");
    }
    else if(S_ISREG(st.st_mode))
    {
        printf("Type: Regular File\n");
    }
    else if(S_ISLNK(st.st_mode))
    {
        printf("Type: Symbolic Link\n");
    }
    else
    {
        printf("Type: Other\n");

    }

}
