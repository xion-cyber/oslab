#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "user/user.h"

/*
    将路径格式化为文件名
*/
char* fmt_name(char *path){
    static char buf[DIRSIZ+1];
    char *p;    // 指针p
    // 从路径末尾开始向左移动，直至指向"/"或路径首字符
    for(p=path+strlen(path); p >= path && *p != '/'; p--);

    p++;    // 指向文件名首字符
    if(strlen(p) >= DIRSIZ)
        return p;
    memmove(buf, p, strlen(p)); // 将文件名复制给buf
    memset(buf+strlen(p), 0, DIRSIZ-strlen(p));   // 将剩余字符置为0
    return buf;
}

/*
    判断文件名和查找文件名是否一致，如果一致则打印路径
*/
void eq_print(char *path, char *findName){
    if(strcmp(fmt_name(path),findName) == 0)
        printf("%s\n", path);
    //printf("path:%s findName:%s\n",fmt_name(path), findName);
    //printf("result:%d\n", strcmp(fmt_name(path),findName));
}


void find(char *path, char *findName){
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;
    
    if((fd = open(path, 0)) < 0){
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    if(fstat(fd, &st) < 0){
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    switch(st.type){
        case T_FILE:    // 如果是文件
            eq_print(path,findName);
            break;
        
        case T_DIR:     // 如果是文件夹，则需要read完所有文件并递归子目录
            if(strlen(path) + 1 + DIRSIZ +1 > sizeof(buf)){
                printf("find: path too long!\n");
                break;
            }
            strcpy(buf, path);
            p = buf + strlen(path);
            *p++ = '/';     // 应为是文件夹，所以要在后面加 "/"

            // 递归读取该文件夹下所有文件和子目录
            while(read(fd, &de, sizeof(de)) == sizeof(de)){
                if(de.inum == 0 || strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
                    continue;       // 如果该dirctory为free，或着为本目录、上级目录，则跳过
                
                memmove(p, de.name, strlen(de.name));
                p[strlen(de.name)] = 0;     // 将文件路径末尾置零

                find(buf, findName);
            }
            break;
    }

    close(fd);
}

int main(int argc, char *argv[]){
    if(argc < 2){
        printf("find: not enough parameters!\n");
        exit(1);
    }

    if(argc < 3){       // 如果只有两个参数，说明就只在本目录下查找
        find(".", argv[1]);
    }
    else
        find(argv[1], argv[2]);     // 在指定的目录下查找
    
    exit(0);

}