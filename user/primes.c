#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// 思路：
// 父进程向子进程中传递需要判断的数字
// 子进程中打印父进程传来的第一个数字
// 然后再次创建子进程，父进程筛去第一个数字的倍数
// 子进程递归
// 这样可以保证每次打印的第一个数字绝对是质数     
void prime(int fp){
    int num;
    read(fp,&num,4);
    printf("prime %d\n", num);      // 打印传来的第一个数字——质数
    int fork_created = 0;

    int pri_fp[2];
    int temp;
    int pid;
    int pri_ret = pipe(pri_fp);
    while(read(fp,&temp,4)){       
        if(!fork_created){
            if(pri_ret == -1){
                printf("pipe create error!\n");
                exit(-1);
            }
            pid = fork();
            fork_created = 1;
            if(pid == 0){       // 如果是子进程就递归
                close(pri_fp[1]);
                prime(pri_fp[0]);
                close(pri_fp[0]);
            }
            else                // 如果是父进程，就关闭读通道
                close(pri_fp[0]);
        }       // 创建子进程
        if(temp%num != 0)
            write(pri_fp[1],&temp,4);
    }
    close(pri_fp[1]);
    wait(0);
    exit(0);

}

int main(int argc, int argv[]){
    int pid;
    int fp[2];
    int ret = pipe(fp);
    if(ret == -1){
        printf("pipe create error!\n");
        exit(-1);
    }
    pid = fork();
    if(pid == 0){
        close(fp[1]);
        prime(fp[0]);
        close(fp[0]);
    }
    else{
        close(fp[0]);
        for(int i=2; i<=35; i++)
            write(fp[1],&i,4);
        close(fp[1]);
    }
    wait(0);
    exit(0);
}
