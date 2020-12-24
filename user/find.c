#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

/**
 * 在xv6上实现UNIX函数find，
 * 即在目录树中查找名称与字符串匹配的所有文件。
*/

char* fmt_name(char *path)
{
  static char buf[DIRSIZ+1];
  char *p;

  //查找最后一个斜杠后的第一个字符。
  for(p=path+strlen(path); p >= path && *p != '/'; p--);
  p++;
  
  memmove(buf, p, strlen(p)+1);  
  return buf;
}

// 当系统文件名与要查找的文件名一致时，打印系统文件完整路径
void equip_path(char *fileName, char *findName){
	if(strcmp(fmt_name(fileName), findName) == 0){
		printf("%s\n", fileName);
	}
}

void find (char *path, char *findName)
{  
  char buf[512], *p;		
  struct dirent de; //目录项结构体  
  struct stat st;   //文件描述符

  int fd = open(path, O_RDONLY);  //打开文件
  fstat(fd, &st);   //由文件描述词取得文件状态

  if (st.type == T_FILE)
  {
    equip_path(path,findName);  //输出完整路径  
  }
  else if (st.type == T_DIR)
  {
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf))
    {
        printf("find: path too long\n");        
    }
    strcpy(buf, path);
    p = buf + strlen(buf);  //指向末尾
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de))
    {
      if(de.inum == 0 || de.inum == 1 || strcmp(de.name,".")==0 || strcmp(de.name,"..")==0){
        continue;
      }          
      memmove(p, de.name, strlen(de.name));   //拷贝p所指的内存内容前n个字节到de.name所指的地址上
      p[strlen(de.name)] = 0;
      find(buf, findName);    //递归查找
    }
  }
  close(fd);
}

int main(int argc, char *argv[])
{  
  if (argc < 3) {
    printf("find: find <path> <fileName>\n");
    exit();
  }
  find(argv[1], argv[2]);    
  exit();
}
