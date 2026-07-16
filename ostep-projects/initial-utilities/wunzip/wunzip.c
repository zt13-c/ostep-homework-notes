#include <stdio.h>
#include <stdlib.h>

int main(int argc,char *argv[]){
	if(argc == 1){
		printf("wunzip: file1 [file2 ...]\n");
      		exit(1);
	}
	int count = 0;
	char cn;
	for(int i = 1;i < argc;i++){
		FILE *fp = fopen(argv[i],"r");
		if(fp == NULL){
			printf("wunzip: cannot open file\n");
            		exit(1);
		}
		while(fread(&count,sizeof(int),1,fp) == 1){
			if(fread(&cn,sizeof(char),1,fp) != 1){
				fclose(fp);
				return 0;
			}
			for(int j = 0;j < count;j++) printf("%c",cn);
		}
		printf("\n");
		fclose(fp);
	}
	return 0;
}
