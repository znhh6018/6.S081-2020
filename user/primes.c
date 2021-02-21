#include "kernel/types.h"
#include "user/user.h"
void create_proc(int num,int *write_fd){
    int pid,p[2],curnum;
    pipe(p);
    pid = fork();
    if(pid < 0){
        printf("fork error\n");
        exit(1);
    }else if(pid == 0){
        int first_num_in_pipe = -1;
        close(p[1]);
        int son_write_fd = -1;
        while(read(p[0],(void*)&curnum,sizeof(curnum)) != 0){
            if(first_num_in_pipe == -1){
                first_num_in_pipe = curnum;
                printf("prime %d\n",curnum);
            }else if(curnum % first_num_in_pipe != 0){
                if(son_write_fd != -1){
                    write(son_write_fd,(void*)&curnum,sizeof(curnum));
                }else{
                    create_proc(curnum,&son_write_fd);
                }
            }
        }
        close(son_write_fd);
        wait(0);
        exit(0);
    }else{
        close(p[0]);
        *write_fd = p[1];
        write(p[1],(void*)&num,sizeof(num));
    }
}
int
main(int argc, char *argv[]){
    if(argc != 1){
        printf("argument error\n");
        exit(1);
    }
    int write_fd = -1;
    for(int i = 2;i < 36;i++){
        if(i == 2){
            printf("prime %d\n",i);
        }else if(i %2 != 0){
            if(write_fd != -1){
                write(write_fd,(void*)&i,sizeof(i));
            }else{
                create_proc(i,&write_fd);
            }
        }
    }    
    close(write_fd);
    wait(0);
    exit(0);
}