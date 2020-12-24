#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"
 
#define MAXARGS 10
#define MAXWORD 30
#define MAXLINE 100

char whitespace[] = " \t\r\n\v";
char args[MAXARGS][MAXWORD];

void execPipe(char*argv[], int argc);

/**
 * echo：输出内容到标准输出
 * ls：显示指定工作目录下之内容
 * grep：搜索指定文件的内容,匹配指定的模式,默认情况下输出匹配内容所在的行
 * cat：连接文件并打印到标准输出设备上
 * 要求支持输入输出重定向 > <
 * 支持两元素管道 | 
*/

/**
 * 从cmd获得输入存在buf里
*/
int getcmd(char *buf, int nbuf)
{
    fprintf(2, "@ ");
    memset(buf, 0, nbuf);
    gets(buf, nbuf);
    if (buf[0] == 0) // EOF
        return -1;
    return 0;
}

/**
 * 将输入的一行命令拆解
*/
void setargs(char *cmd, char* argv[], int* argc)
{
    // 让argv的每一个元素都指向args的每一行
    for(int i = 0; i < MAXARGS; i++){
        argv[i]=&args[i][0];
    }
    int i;  // 表示第i个word    
    int j = 0;
    for (i = 0; cmd[j] != '\n' && cmd[j] != '\0'; j++)
    {
        // 每一轮循环都是找到输入的命令中的一个word，比如 echo hi ,就是先找到echo，再找到hi
        // 让argv[i]分别指向他们的开头，并且将echo，hi后面的空格设为\0        
        while (strchr(whitespace, cmd[j]))  //在一个串中查找给定字符的第一个匹配之处
        {// 跳过之前的空格
            j++;
        }
        argv[i++]=cmd+j;
        
        while (strchr(whitespace,cmd[j])==0)
        {// 只要不是空格，就j++,找到下一个空格
            j++;
        }
        cmd[j]='\0';
    }
    argv[i]=0;
    *argc=i;
}
 
void runcmd(char* argv[], int argc)
{
    for(int i = 1; i < argc; i++)
    {
        if(!strcmp(argv[i], "|"))
        {// 如果遇到 | 即pipe，说明后面还有一个命令要执行            
            execPipe(argv,argc);
        }
    }
    // 此时是仅处理一个命令：现在判断argv[1]开始，后面有没有> 
    for(int i=1;i<argc;i++){
        // 如果遇到 > ，说明需要执行输出重定向，首先需要关闭stdout
        if(!strcmp(argv[i],">")){
            close(1);
            // 此时需要把输出重定向到后面给出的文件名对应的文件里
            // 当然如果>是最后一个，那就会error，不过暂时先不考虑
            open(argv[i+1],O_CREATE|O_WRONLY);
            argv[i]=0;            
        }
        if(!strcmp(argv[i],"<")){
            // 如果遇到< ,需要执行输入重定向，关闭stdin
            close(0);
            open(argv[i+1],O_RDONLY);
            argv[i]=0;            
        }
    }
    exec(argv[0], argv);
}

/**
 * 执行pipe
*/
void execPipe(char*argv[],int argc){
    int i=0;
    // 首先找到命令中的"|",然后把他换成'\0'
    // 从前到后，找到第一个就停止，后面都递归调用
    for(;i<argc;i++){
        if(!strcmp(argv[i],"|")){
            argv[i]=0;
            break;
        }
    }
    // 先考虑最简单的情况：cat file | wc
    int fd[2];
    pipe(fd);
    if(fork()==0){
        // 子进程 执行左边的命令 把自己的标准输出关闭
        close(1);
        dup(fd[1]);
        close(fd[0]);
        close(fd[1]);
        // exec(argv[0],argv);
        runcmd(argv,i);
    }else{
        // 父进程 执行右边的命令 把自己的标准输入关闭
        close(0);
        dup(fd[0]);
        close(fd[0]);
        close(fd[1]);
        // exec(argv[i+1],argv+i+1);
        runcmd(argv+i+1,argc-i-1);
    }
}

int main()
{
    char buf[MAXLINE];        
    while (getcmd(buf, sizeof(buf)) >= 0)
    {
        if (fork() == 0)
        {// 子进程            
            char* argv[MAXARGS];    //每一个指针都指向拆解后的单词
            int argc= -1;           //argv数组的个数
            setargs(buf, argv, &argc);
            runcmd(argv,argc);      //跑指令
        }
        wait(0);
    } 
    exit(0);
}