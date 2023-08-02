#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int sec;
    
    // 检查命令行参数个数
    if (argc <= 1)
    {
        printf("usage: sleep [seconds]\n");
        exit(0);
    }
    
    // 将命令行参数转换为整数
    sec = atoi(argv[1]);
    
    // 调用sleep函数进行相应秒数的休眠
    sleep(sec);
    
    // 正常退出程序
    exit(0);
}
