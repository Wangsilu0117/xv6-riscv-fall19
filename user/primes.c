#include "kernel/types.h"
#include "user/user.h"

/*
* 传入要筛选的primes[]
*/
void func(int *input, int num){
	if(num == 1){
		// 最后一次递归之后，输出31，然后就返回不递归了
		printf("prime %d\n", *input);
		return;
	}
	int p[2],i;
	int prime = *input;
	int temp;	
	// 非最后一次递归
	printf("prime %d\n", prime);
	pipe(p);
	// 第一个子进程负责处理数据
    if(fork() == 0){
		// 子，将当前要进行判断的primes传入pipe
        for(i = 0; i < num; i++){
            temp = *(input + i);	//input+i == input[i] 的地址，temp == input[i]			
			//&temp == input + i == input[i]的地址						
			//每一次写不会覆盖吗？ 不会，p[1]相当于一个excel，会顺序写下去!					
			write(p[1], (int *)(&temp), 2);		//写入input[i],input[i+1]
		}
        exit();
    }
	//父
	close(p[1]);	//关闭打开的fd->p[1]
	// 第二个子进程负责筛选数据
	if(fork() == 0){
		// 子
		int counter = 0;	//记录不能被prime整除的数的个数
		int buffer[2];		//buffer一定要是一个数组！
		while(read(p[0], buffer, 2) != 0){	//取得顺序不会错！因为是对应的一次取2个!只不过是只用了1个！
			temp = *(buffer);	//取到了input[i]			
			if(temp % prime != 0){
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
