#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

void recursive_find(const char *start_path)
{
    struct stat st;

    if(stat(start_path,&st) < 0)
    {
        perror("stat");
        return;
    }

    printf("%s\n",start_path);

    if(!S_ISDIR(st.st_mode)) return;

    DIR *dir = opendir(start_path);

    if(dir == NULL)
    {
        perror("opendir");
        return;
    }

    struct dirent *entry;

    while((entry = readdir(dir)) != NULL)
    {
        if(strcmp(entry->d_name,".") == 0 || strcmp(entry->d_name,"..") == 0) continue;

        char path[PATH_MAX];
        snprintf(path,sizeof(path),"%s/%s",start_path,entry->d_name);

        recursive_find(path);
    }

    closedir(dir);
}


int main(int argc,char *argv[])
{
    const char *start_path;

    if(argc == 1) start_path = ".";
    else if(argc == 2) start_path = argv[1];
    else
    {
        fprintf(stderr,"Usage: %s [directory]\n",argv[0]);
        exit(1);
    }
    
    recursive_find(start_path);
    return 0;
}