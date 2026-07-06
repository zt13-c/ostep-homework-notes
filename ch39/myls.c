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

void print_long_info(const char *dirpath, const char *filename)
{
    struct stat st;
    char fullpath[PATH_MAX];
    snprintf(fullpath,sizeof(fullpath),"%s/%s",dirpath,filename);
    if(stat(fullpath,&st) == -1)
    {
        perror("stat");
        exit(1);
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

void list_directory(const char *dirpath, int long_format)
{
    struct dirent *entry;
    DIR *dir = opendir(dirpath);
    if(dir == NULL)
    {
        perror("opendir");
        exit(1);
    }
    while((entry = readdir(dir)) != NULL)
    {
        if(entry -> d_name[0] == '.') continue;
        if(long_format) print_long_info(dirpath,entry -> d_name);
        else printf("%s\n",entry -> d_name);
    }
    closedir(dir);

}

int main(int argc, char *argv[])
{
    int long_format = 0;
    char dirpath[PATH_MAX];

    if(argc == 1)
    {
        if(getcwd(dirpath,sizeof(dirpath)) == NULL)
        {
            perror("getcwd");
            exit(1);
        }
    }
    else if(argc == 2)
    {
        if(strcmp(argv[1],"-l") == 0)
        {
            long_format = 1;
            if(getcwd(dirpath,sizeof(dirpath)) == NULL)
            {
                perror("getcwd");
                exit(1);
            }
        }
        else
        {
            strncpy(dirpath,argv[1],sizeof(dirpath));
            dirpath[sizeof(dirpath) - 1] = '\0';
        }
    }
    else if(argc == 3)
    {
        if(strcmp(argv[1],"-l") == 0)
        {
            long_format = 1;
            strncpy(dirpath,argv[2],sizeof(dirpath));
            dirpath[sizeof(dirpath) - 1] = '\0';
        }
        else
        {
            fprintf(stderr,"Usage: %s [-l] [directory]\n",argv[0]);
            exit(1);
        }
    }
    else
    {
        fprintf(stderr,"Usage: %s [-l] [directory]\n",argv[0]);
        exit(1);
    }

    list_directory(dirpath,long_format);

    return 0;
}