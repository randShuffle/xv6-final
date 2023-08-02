#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

char buf[128];


int main(int argc, char *argv[])
{
    int p1[2], p2[2], pid;
    char buf[1]; // 用于存储传输的数据

    pipe(p1); // 创建管道p1
    pipe(p2); // 创建管道p2

    if (fork() == 0) // 子进程
    {
        close(p2[1]); // 关闭子进程的p2写入端
        close(p1[0]); // 关闭子进程的p1读取端
        pid = getpid();

        int num_read = read(p2[0], buf, 1); // 从p2读取数据
        if (num_read == 1) {
            printf("%d: received ping\n", pid);
            write(p1[1], buf, 1); // 向p1写入数据
        }
    }
    else // 父进程
    {
        close(p1[1]); // 关闭父进程的p1写入端
        close(p2[0]); // 关闭父进程的p2读取端
        pid = getpid();

        write(p2[1], buf, 1); // 向p2写入数据
        int num_read = read(p1[0], buf, 1); // 从p1读取数据
        if (num_read == 1) {
            printf("%d: received pong\n", pid);
        }
    }

    exit(0);
}
