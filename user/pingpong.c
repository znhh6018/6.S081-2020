#include "kernel/types.h"
#include "user/user.h"
int
main(int argc, char *argv[])
{
    if(argc != 1){
        printf("argument error \n");
    }
    char buf[1];
    int p[2],pid;
    pipe(p);
    pid = fork();
    if(pid < 0){
        printf("fork error\n");
        exit(1);
    }else if(pid == 0){
        close(p[1]);
        read(p[0],buf,1);
        printf("%d: received ping\n",getpid());
        exit(0);
    }else{
        close(p[0]);
        write(p[1],"1",1);
        wait(0);
        printf("%d: received pong\n",getpid());
        exit(0);
    }
}
