#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main()
{
    int pid = fork();
    if (pid == 0)
    {
        printf("Child Process\n");
        long long int counter = 0;
        for (long long int i = 0; i < 10000000000; i++)
            counter++;
        printf("done\n");
        struct proc_info po;
        cpu_usage(&po);
        printf("id: %d ticks: %d\n", po.pid, po.usage.sum_of_ticks);
        exit(0);
    }
    sleep(120);
    printf("Parent Process\n");
    long long int counter = 0;
    for (long long int i = 0; i < 10000000000; i++)
        counter++;
    struct proc_info po;
    cpu_usage(&po);
    printf("id: %d ticks: %d\n", po.pid, po.usage.sum_of_ticks);
    exit(0);
}