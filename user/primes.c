#include "kernel/types.h"
#include "user/user.h"

/*
* 传入要筛选的primes[]
*/
void func(int *input, int num)
{		
	int p[2],i,temp;
	int prime = *input;
	// 非最后一次递归
	printf("prime %d\n", prime);	
	if(num == 1)
	{// 最后一次递归之后，返回				
		return;
	}
	
	pipe(p);
	
    if(fork() == 0)		
	{// 子，将当前要进行判断的primes传入pipe		
        for(i = 0; i < num; i++){
            temp = *(input + i);	
			//虽然写入两个数据，实际上只取第一个
			write(p[1], (int *)(&temp), 2);		
		}
        exit();
    }
	
	close(p[1]);
	
	if(fork() == 0)
	{// 第二个子进程负责筛选数据		
		int counter = 0;	//记录不能被prime整除的数的个数
		int buffer[2];		//buffer一定要是一个数组！
		while(read(p[0], buffer, 2) != 0)
		{
			temp = *(buffer);
			if(temp % prime != 0)
			{//是潜质数，需要进入下一次筛选
				*input = temp;
				input += 1;
				counter++;
			}
		}
		func(input - counter, counter);
		exit();
    }		
    // 需要使用两个wait()!
	wait();
    wait();
}

int main(int argn, char *argv[]){
    int input[34];	
	for(int i = 0; i < 34; i++){
		input[i] = i+2;
	}
	func(input, 34);	
    exit();
}
