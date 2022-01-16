/**
 * @date: 2022/1/16
 * @author: RayleighZ
 * @describe: 延时函数，调用已有的sleep
 */
#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    int sec;
    if(argc < 2){
        fprintf(2, "Usage: sleep seconds...\n");
        exit(1);
    }
    sec = atoi(argv[1]);
    sleep(sec);
    exit(0);
}
