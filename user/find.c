#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

// 函数：fmtname
// 功能：从路径中获取文件名
// 参数：path - 文件路径字符串指针
// 返回值：文件名字符串指针
char *fmtname(char *path)
{
    char *p;
    // 找到最后一个斜杠后的第一个字符
    for (p = path + strlen(path); p >= path && *p != '/'; p--)
        ;
    p++;
    return p;
}

// 函数：find
// 功能：递归查找指定路径下的指定文件
// 参数：path - 查找路径字符串指针
//       filename - 目标文件名字符串指针
void find(char *path, char *filename)
{
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;
    if ((fd = open(path, 0)) < 0)
    {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    if (fstat(fd, &st) < 0)
    {
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    switch (st.type)
    {
    case T_FILE:
        if (strcmp(fmtname(path), filename) == 0)
        {
            printf("%s\n", path);
        }
        break;

    case T_DIR:
        if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf)
        {
            printf("find: path too long\n");
            break;
        }
        strcpy(buf, path);
        p = buf + strlen(buf);
        *p++ = '/';
        while (read(fd, &de, sizeof(de)) == sizeof(de))
        {
            if (de.inum == 0)
                continue;
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;
            if (stat(buf, &st) < 0)
            {
                printf("find: cannot stat %s\n", buf);
                continue;
            }
            if (strcmp(fmtname(buf), ".") != 0 && strcmp(fmtname(buf), "..") != 0)
            {
                find(buf, filename);
            }
        }
        break;
    }
    close(fd);
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("usage: find [path] [filename]\n");
        exit(0);
    }
    find(argv[1], argv[2]);
    exit(0);
}
