#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void test(long long number)
{
    long long counter = 0;
    for (long long i = 0; i < number; i++)
    {
        counter++;
    }
    printf("%lld\n", counter);
}

int main()
{
    int pid = fork2(10);
    if (pid == 0)
    {
        test(1000000000);
        exit(0);
    }

    sleep(50);
    struct top t;
    top(&t);
    printf("number of processes: %d\n", t.count);
    printf("PID\t\tPPID\t\tSTATE\t\tNAME\t\tSTART\t\tUSAGE\n");

    for (int i = 0; i < t.count; i++)
    {
        printf("%d\t\t", t.processes[i].pid);
        printf("%d\t\t", t.processes[i].ppid);
        switch (t.processes[i].state)
        {
        case UNUSED:
            printf("UNUSED\t\t");
            break;
        case USED:
            printf("USED\t\t");
            break;
        case SLEEPING:
            printf("SLEEPING\t\t");
            break;
        case RUNNABLE:
            printf("RUNNABLE\t\t");
            break;
        case RUNNING:
            printf("RUNNING\t\t");
            break;
        default:
            printf("ZOMBIE\t\t");
            break;
        }
        printf("%s\t\t", t.processes[i].name);
        printf("%d\t\t", t.processes[i].usage.start_tick);
        printf("%d\n", t.processes[i].usage.sum_of_ticks);
    }
    return 0;
}