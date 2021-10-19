#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char* argv[]){
    int parent_fd[2];
    int child_fd[2];
    int parent = pipe(parent_fd);
    int child = pipe(child_fd);

    if(parent == -1 || child == -1){
        printf("pipe error\n");
        exit(-1);
    }
    // 如果创建失败，则打印消息并退出

    int pid = fork();
    char buf[10];

    if(pid == 0){
        read(parent_fd[0],buf,4);
        // 读取父进程的消息，否则阻塞
        printf("%d: received %s\n",getpid(),buf);
        write(child_fd[1],"pong",4);
        // 将pong写入管道
    }
    else{
        write(parent_fd[1],"ping",4);
        // 将ping写入管道
        read(child_fd[0],buf,4);
        // 从子进程中读取消息
        printf("%d: received %s\n",getpid(),buf);
    }

    close(parent_fd[0]);
    close(parent_fd[1]);
    close(child_fd[0]);
    close(child_fd[1]);
    wait(0); // 父进程等待子进程退出
    exit(0);

}
