/**
 * @date: 2022/1/16
 * @author: RayleighZ
 * @describe: pipe练习
 */
#include "kernel/types.h"
#include "user/user.h"

int areStringSame(char a[], char b[], int count){
    int i = 0;
    for (i = 0; i < 4; i++) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

int main(int argc, char argv[]) {
    int p[2];
    char *ping = "ping";
    char *pong = "pong";
    char buff[256];
    if (argc < 1) {
        fprintf(2, "Usage: ping, and wait for pong\n");
        exit(1);
    }
    if (pipe(p) < 0) {//形成管道
        fprintf(2, "Error: can't format pipe\n");
        exit(1);
    }
    if (fork() == 0) {//如果位于子进程中，则等待收到ping，之后输出pong
        //等待管道输出
        read(p[0], buff, 4);
        if (!areStringSame(buff, ping, 4)) {
            fprintf(2, "Error: not ping\n");
            exit(1);
        }
        printf("<%d> receive ping\n", getpid());
        //向管道中写入"pong"
        write(p[1], pong, 4);
        //关闭管道
        close(p[0]);
        close(p[1]);
    } else {
        //向管道中写入"ping"
        write(p[1], ping, 4);
        //等待管道输出
        read(p[0], buff, 4);
        if (!areStringSame(buff, pong, 4)) {
            fprintf(2, "Error: not pong\n");
            exit(1);
        }
        printf("<%d> receive pong\n", getpid());
        //关闭管道
        close(p[0]);
        close(p[1]);
    }
    exit(0);
}

