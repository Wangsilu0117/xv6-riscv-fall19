#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]){
    int i,k;
    int j = 0,l = 0,m = 0;        
    char block[32],buf[32];    
    char *p = buf;
    char *lineSplit[32];    
    for(i = 1; i < argc; i++){
        lineSplit[j++] = argv[i];
    }
    while( (k = read(0, block, sizeof(block))) > 0)//从文件中读n个字节到buf
    {
        for(l = 0; l < k; l++)
        {
            if(block[l] == '\n')
            {// 新的一行                
                buf[m] = 0;
                m = 0;
                lineSplit[j++] = p;
                p = buf;
                lineSplit[j] = 0;
                j = argc - 1;                
                if(fork() == 0)
                {// 对每一行输入调用命令
                    exec(argv[1], lineSplit);
                }                
                wait();
                
            }
            else if(block[l] == ' ') 
            {
                buf[m++] = 0;
                lineSplit[j++] = p;
                p = &buf[m];
            }
            else 
            {
                buf[m++] = block[l];
            }
        }
    }
    exit();
}
