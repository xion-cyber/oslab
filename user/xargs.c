#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

/*
    思路：利用一个字符串数组来存储输入的参数
         利用一个指针来区分命令行参数和标准输入
         利用fork来创建子进程，在子进程中执行
         MAXARG为32，为每个参数分配32个字节长度，共1M空间
*/

int main(int argc, char *argv[]){
    char buf[1024], *arguments[MAXARG],ch;
    // buf_p 用来指示当前输入存储在buf[]中的位置
    // buf_s 用来存储命令行参数最后一个字符所在位置
    // arguments_s 用来存储最后一个命令行参数在arguments[]中的位置
    // arguments_cnt 用来记录arguments[]里面一共有多少个参数(-1)
    int buf_s = 0, buf_p, arguments_s = 0, arguments_cnt; 

    if(argc < 3){
        printf("xargs: not enough parameters!\n");
        exit(0);
    }

    int i;
    for( i=1; i < argc; i++){
        int j;
        arguments[arguments_s++] = buf + buf_s;       // 用来记录参数字符串的首地址
        for( j=0; j<strlen(argv[i]); j++){
            buf[buf_s++] = argv[i][j];
        }
        buf[buf_s++] = 0;       // 需要在每一个参数后面补上字符串终止符
    }

    arguments[arguments_s++] = buf + buf_s;
    buf_p = buf_s;
    arguments_cnt = arguments_s;

    // 从标准输入中读取参数

    while( read( 0, &ch, 1) >0 ){
        // 如果读入的是回车，那么代表此时一行的参数已经输完，需要执行程序了
        if( ch == '\n'){
            buf[buf_p++] = 0;
            arguments[arguments_cnt++] = 0;
            if( fork() == 0 ){
                exec( argv[1], arguments);
            }
            else{
                wait(0);
                arguments_cnt = arguments_s;
                buf_p = buf_s;
            }
        }
        // 如果输入的是空格，那么代表一个参数已经输入完毕，准备读取下一个参数
        else if( ch == ' ' ){
            buf[buf_p++] = 0;
            arguments[arguments_cnt++] = buf + buf_p;
        }
        else{
            buf[buf_p++] = ch;
        }
    }

    exit(0);

}