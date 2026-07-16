#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void search_stream(char *searchterm,FILE *fp){
  char *line = NULL;
  size_t len = 0;
  while(getline(&line,&len,fp) != -1){
    if(strstr(line,searchterm) != NULL) printf("%s",line);
  }
  free(line);
}

int main(int argc,char *argv[])
{
  if(argc == 1){
    printf("wgrep: searchterm [file ...]\n");
    exit(1);
  }
  char *searchterm = argv[1];
  if(argc == 2){
    search_stream(searchterm,stdin);
    return 0;
  }
  for(int i = 2;i < argc;i++){
    FILE *fp = fopen(argv[i],"r");
    if(fp == NULL){
      printf("wgrep: cannot open file\n");
      exit(1);
    }
    search_stream(searchterm,fp);
    fclose(fp);
  }
  return 0;
}
