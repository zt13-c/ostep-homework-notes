#include<stdio.h>
#include<stdlib.h>
int main(int argc,char *argv[])
{
   if(argc == 1) return 0;
   for(int i = 1;i < argc;i++){
	FILE *p = fopen(argv[i],"r");
	if(p == NULL){
	   printf("wcat: cannot open file\n");
	   exit(1);
	}

	char buffer[4096];
	while(fgets(buffer,sizeof(buffer),p) != NULL){
	     printf("%s",buffer);
	}
	fclose(p);
   }
   return 0;
}
