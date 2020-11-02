#include "kernel/types.h"
#include "user/user.h"

int main(int argn, char *argv[]){
	int NumOut;
    int prime[34];
    int buf[34];    
    int father_pipe[2], son_pipe[2], father_length_pipe[2], son_length_pipe[2];
    int length;
    int time = 0;

    pipe(father_pipe);          //管道：父进程写数组数据到子进程 p1
    pipe(son_pipe);             //管道：子进程写数组数据到父进程 s1
    pipe(father_length_pipe);   //管道: 父进程写数组长度到子进程 p1
    pipe(son_length_pipe);      //管道: 子进程写数组长度到父进程 s1
                    
    //循环条件待修改       
    while (time < 12)
    {
        if (fork() == 0) {                
        //子进程        
            close(father_pipe[1]);
            close(father_length_pipe[1]);
            close(son_length_pipe[0]);
            close(son_pipe[0]);

            if (time == 0)
            {
                //初始化操作直接传给父进程                
                for (int i = 0; i < 34; i++)
                {
                    //from 2 to 35
                    prime[i] = i+2;
                }
                length = 34;
                buf[0] = length;
                write(son_length_pipe[1],buf,1);  //子进程将新新的length 和 prime数组 数据传给父进程
                write(son_pipe[1],prime,length);
            }
            else {
            //不是第一次
                read(father_length_pipe[0],buf,1);     //子进程读出新的prime数组的长度
                length = buf[0];                    
                read(father_pipe[0],buf,length);    //子进程读出新的prime数组的数据,存到buf
                NumOut = buf[0];    
                printf("%d\n",NumOut);              //子进程打印质数
                int k = 0;  //计数
                for (int i = 0; i < length; i++)    //子进程筛选出新新的prime数据
                {
                    if (buf[i] % NumOut != 0)
                    {
                        prime[k] = buf[i];
                        k++;
                    }                
                }
                length = k;  
                buf[0] = length;
                
                write(son_length_pipe[1],buf,1);  //子进程将新新的length 和 prime数组 数据传给父进程
                write(son_pipe[1],prime,length);
            }    
            
            exit();
        }
                
        close(father_pipe[0]);
        close(father_length_pipe[0]);
        close(son_pipe[1]);
        close(son_length_pipe[1]);
        
        // wait();
        read(son_length_pipe[0],buf,1);  //从子进程中读出prime数组的长度
        length = buf[0];
        read(son_pipe[0],prime,length);     //从子进程中读出prime数组的内容
        
        write(father_length_pipe[1],buf,1);    //将新prime数组的长度写回给子进程
        write(father_pipe[1],prime,length); //将新prime数组的数据写回给子进程
        
        time++;
    }
    
    exit();

}


