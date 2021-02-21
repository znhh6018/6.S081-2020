#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/param.h"

int main(int argc,char*argv[]){
    if(argc < 2){
        printf("argument error\n");
        exit(0);
    }
    char buf[1024],*p;
    p = buf;
    char* new_argv[MAXARG]; 
    int command_idx = 1;
    if(strcmp("-n",argv[1]) == 0){
        command_idx = 3;
    }
    int argv_idx = 0;
    for(int i = command_idx;i < argc;i++){
        new_argv[argv_idx++] = argv[i];
    }
    int params_in_xargs = argv_idx;
    new_argv[argv_idx++] = p;
    while(read(0,p,1) > 0 ){
        if(*p == '\n'){
            *p = 0;
            new_argv[argv_idx] = 0;
            int pid = fork();
            if(pid < 0){
                printf("fork error\n");
                exit(1);
            }else if(pid == 0){
                exec(new_argv[0],new_argv);
                printf("exec fail\n");
                exit(1);
            }
            wait(0);
            for(int i = params_in_xargs;i < argv_idx;i++){
                new_argv[i] = 0;
            }
            argv_idx = params_in_xargs;
            memset((void*)buf,0,512);
            p = buf;
            new_argv[argv_idx++] = p;
        }else if (*p == ' '){
            *p++ = 0;
            new_argv[argv_idx++] = p;
        }else{
            p++;
        }
    }
    exit(0);
}