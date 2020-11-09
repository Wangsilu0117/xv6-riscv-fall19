#include "kernel/types.h"
#include "user/user.h"

/**
* 管道pipe：正常创建后，xxx[1]为管道写入端，xxx[0]为管道读出端
* fork()的返回值：子进程的返回值为0，父进程的返回值为子进程的pid号
* 爱了，这个pipe的返回值并不需要用上，in fact，返回值也是-1 表示错误，0表示正确
*/
int main(int argn, char *argv[]){
	int parent_fd[2], child_fd[2];

    char parent_buf[4] = {'p','i','n','g'};    
    char parent_get[4];
    char child_buf[4] = {'p','o','n','g'};      
    char child_get[4];
    int length = sizeof(parent_buf);    
    pipe(parent_fd); 
    pipe(child_fd); 
    
    if (fork() == 0) {                
    //子进程
        close(parent_fd[1]);
        close(child_fd[0]);
        read(parent_fd[0],child_get, length);    //子进程从 pipe parent_fd[0] 中接收 ping 到 child_get
        printf("%d: received %s\n",getpid(),child_get);
        write(child_fd[1],child_buf,length);     //子进程发送 pong 到 pipe child_fd[1]
        exit();    
    }
    //父进程
    close(parent_fd[0]);
    close(child_fd[1]);
    write(parent_fd[1], parent_buf, length);     //父进程发送 ping 到 pipe parent_fd[1]
    read(child_fd[0],parent_get, length);         //父进程从pipe child_fd[0] 中读出 pong 到parent_get
    printf("%d: received %s\n",getpid(),parent_get);          
    exit();
}
