#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"



const int MAX_NUM = 35;

int p1[2], fdr, fdw;
long p, n;
int is_first = 1;

int main(int argc, char *argv[])
{
    if (is_first == 1)
    {
        is_first = 0;
        pipe(p1);   // 创建管道 p1
        fdr = p1[0];   // 设置 fdr 为管道 p1 的读端
        fdw = p1[1];   // 设置 fdw 为管道 p1 的写端
        for (n = 2; n <= MAX_NUM; n++)
        {
            write(fdw, (void *)&n, 8);   // 将数字 n 写入 fdw
        }
        close(fdw);   // 关闭 fdw
    }

    if (fork() == 0)   // 子进程
    {
        if (read(fdr, (void *)&p, 8))
        {
            printf("prime %ld\n", p);
            pipe(p1);   // 创建新的管道 p1
            fdw = p1[1];   // 设置 fdw 为新管道 p1 的写端
        }

        while (read(fdr, (void *)&n, 8))
        {
            if (n % p != 0)
                write(fdw, (void *)&n, 8);   // 将 n 写入 fdw
        }
        fdr = p1[0];   // 将 fdr 设置为新管道 p1 的读端
        close(fdw);   // 关闭 fdw
        main(argc, argv);   // 递归调用 main 函数
    }
    else   // 父进程
    {
        wait((int *)0);   // 等待子进程结束
        close(fdr);   // 关闭 fdr
    }

    exit(0);   // 退出程序
}
