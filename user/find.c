#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
void recur_find(char*dir_path,char*filename){
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;
    if((fd = open(dir_path, 0)) < 0){
        fprintf(2, "ls: cannot open %s\n", dir_path);
        exit(1);
    }
    if(fstat(fd, &st) < 0){
        fprintf(2, "ls: cannot stat %s\n", dir_path);
        close(fd);
        exit(1);
    }
    if(strlen(dir_path) + 1 + DIRSIZ + 1 > sizeof buf){
        printf("ls: path too long\n");
        exit(1);
    }
    strcpy(buf, dir_path);
    p = buf+strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
        if(de.inum == 0)
            continue;
        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;
        if(stat(buf, &st) < 0){
            printf("ls: cannot stat %s\n", buf);
            continue;
        }
        switch(st.type){
            case T_FILE:
                if(strcmp(de.name,filename) == 0){
                    printf("%s\n",buf);
                }
                break;
            case T_DIR:

                if(strcmp(de.name,".") == 0 || strcmp(de.name,"..") == 0){
                    break;
                }
                recur_find(buf,filename);
                break;
      }
    }
}

int main(int argc,char*argv[]){
    if(argc != 3){
        printf("argument error\n");
        exit(1);
    }
    char*dir_path = argv[1],*filename = argv[2];
    int fd;
    struct stat st;

    if((fd = open(dir_path, 0)) < 0){
        fprintf(2, "ls: cannot open %s\n", dir_path);
        exit(1);
    }

    if(fstat(fd, &st) < 0){
        fprintf(2, "ls: cannot stat %s\n", dir_path);
        close(fd);
        exit(1);
    }
    switch(st.type){
        case T_FILE:
            printf("it's not a directory\n");
            exit(1);
        case T_DIR:
            recur_find(dir_path,filename);
            break;
    }
    exit(0);
}