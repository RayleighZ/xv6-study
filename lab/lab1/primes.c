/**
 * @date: 2022/1/18
 * @author: RayleighZ
 * @describe: 管道练习2，质数筛
 */
#include "kernel/types.h"
#include "user/user.h"
int main(){
    int p[2];//管道
    int i;
    int allPrimes = 0;
    int receiveBuf[36];
    int receiveCount;
    int buf;
    int startNum;
    pipe(p);
    if(fork() == 0){
        do {
            close(p[1]);
            receiveCount = 0;
            while (read(p[0], &buf, 1)){
                if(receiveCount == 0){
                    startNum = buf;
                    receiveBuf[receiveCount] = buf;
                    receiveCount++;
                    continue;
                }
                if(buf % startNum != 0){
                    //质数就添加进去
                    receiveBuf[receiveCount] = buf;
                    receiveCount++;
                }
            }
            //如果一个都没收到，就找完质数了
            allPrimes = receiveCount != 0;
            if(allPrimes != 0){
                //输出第一个
                printf("prime %d\n", receiveBuf[0]);
                //刷新管道
                pipe(p);
                //向下一个进程发包
                if(fork() != 0){
                    close(p[0]);
                    for (i = 1; i < receiveCount; ++i) {
                        write(p[1], &receiveBuf[i], 1);
                    }
                    close(p[1]);
                    exit(0);
                }
            }
        } while (allPrimes != 0);
        exit(0);
    } else {
        close(p[0]);
        //在父进程中顺序发送2~36
        for (i = 2; i < 37; ++i) {
            write(p[1], &i, 1);
        }
        //结束发送，关闭管道
        close(p[1]);
        exit(0);
    }
}

