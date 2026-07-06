#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#define BUF_SIZE 4096

int main(int argc,char *argv[])
{
    if(argc != 3)
    {
        fprintf(stderr,"Usage: %s -n file\n",argv[0]);
        exit(1);
    }

    int n = atoi(argv[1]+1);
    if(argv[1][0] != '-' || n <= 0)
    {
        fprintf(stderr,"Usage: %s -n file\n",argv[0]);
        exit(1);
    }
    
    char *filename = argv[2];

    int fd = open(filename,O_RDONLY);
    if(fd == -1)
    {
        perror("open");
        exit(1);    
    }

    struct stat st;
    if(stat(filename,&st) < 0)
    {
        perror("stat");
        close(fd);
        exit(1);
    }

    off_t file_size = st.st_size;
    off_t pos = file_size;
    int lines = 0;
    char c;

    while(pos > 0 && lines < n)
    {
        pos--;
        if(lseek(fd,pos,SEEK_SET) < 0)
        {
            perror("lseek");
            close(fd);
            exit(1);
        }
        if(read(fd,&c,1) != 1)
        {
            perror("read");
            close(fd);
            exit(1);
        }
        if(c == '\n')
        {
            if(pos != file_size -1) lines++;
            if(lines == n)
            {
                pos++;
                break;
            }
        }
    }

    if (pos == 0) lseek(fd, 0, SEEK_SET);
    else  lseek(fd, pos, SEEK_SET);

    char buf[BUF_SIZE];
    ssize_t bytes_read;

    while((bytes_read = read(fd, buf, BUF_SIZE)) > 0)
    {
        if(write(STDOUT_FILENO, buf, bytes_read) != bytes_read)
        {
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