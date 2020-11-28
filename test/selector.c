#include<stdio.h>
#include<stdlib.h>
#include<sys/syscall.h>
#include<unistd.h>

#define SYS_ROTLOCK_WRITE 400
#define SYS_ROTUNLOCK_WRITE 402

int main(int argc, char* argv[]){

    FILE* fp;
    if(argc != 2){
        printf("wrong input\n");
        return 0;
    }
    int num = atoi(argv[1]);
    while(1){
        if(syscall(SYS_ROTLOCK_WRITE(90,90))==-1){
            printf("degree is wrong\n");
            return 0;
        }
        fp = fopen("integer",'w');
        fprintf(fp,"%d", num);
        printf("selector: %d\n",num);
        fclose(fp);
        num++;
        if(syscall(SYS_ROTUNLOCK_WRITE(90,90))==-1){
            printf("unlock is wrong\n");
            return 0;
        }
    }
    return 0;
}